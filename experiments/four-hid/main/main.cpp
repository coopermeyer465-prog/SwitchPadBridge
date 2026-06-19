#include <cstring>
#include <cstdlib>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "lwip/ip_addr.h"
#include "nvs_flash.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"
#include "secrets.h"
#include "web_ui.h"

static constexpr uint8_t kControllerCount = 4;
static constexpr uint32_t kInputTimeoutMs = 900;
static constexpr uint32_t kClientLeaseMs = 15000;
static const char *kTag = "switchpad-4hid";

struct SwitchReport {
  uint16_t buttons;
  uint8_t hat;
  uint8_t lx;
  uint8_t ly;
  uint8_t rx;
  uint8_t ry;
  uint8_t vendor;
} __attribute__((packed));

static SwitchReport reports[kControllerCount];
static uint32_t last_input_ms[kControllerCount];
static portMUX_TYPE report_lock = portMUX_INITIALIZER_UNLOCKED;

struct ClientSlot {
  char device_id[40];
  uint32_t last_seen_ms;
};

static ClientSlot clients[kControllerCount] = {};

static const uint8_t hid_report_descriptor[] = {
  0x05, 0x01, 0x09, 0x05, 0xA1, 0x01,
  0x15, 0x00, 0x25, 0x01, 0x35, 0x00, 0x45, 0x01,
  0x75, 0x01, 0x95, 0x10, 0x05, 0x09, 0x19, 0x01,
  0x29, 0x10, 0x81, 0x02, 0x05, 0x01, 0x25, 0x07,
  0x46, 0x3B, 0x01, 0x75, 0x04, 0x95, 0x01, 0x65, 0x14,
  0x09, 0x39, 0x81, 0x42, 0x65, 0x00, 0x95, 0x01, 0x81, 0x01,
  0x26, 0xFF, 0x00, 0x46, 0xFF, 0x00, 0x09, 0x30, 0x09, 0x31,
  0x09, 0x32, 0x09, 0x35, 0x75, 0x08, 0x95, 0x04, 0x81, 0x02,
  0x06, 0x00, 0xFF, 0x09, 0x20, 0x95, 0x01, 0x81, 0x02,
  0x0A, 0x21, 0x26, 0x95, 0x08, 0x91, 0x02, 0xC0
};

static const tusb_desc_device_t device_descriptor = {
  .bLength = sizeof(tusb_desc_device_t),
  .bDescriptorType = TUSB_DESC_DEVICE,
  .bcdUSB = 0x0200,
  .bDeviceClass = 0x00,
  .bDeviceSubClass = 0x00,
  .bDeviceProtocol = 0x00,
  .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
  .idVendor = 0x0F0D,
  .idProduct = 0x0092,
  .bcdDevice = 0x0200,
  .iManufacturer = 0x01,
  .iProduct = 0x02,
  .iSerialNumber = 0x03,
  .bNumConfigurations = 0x01,
};

static char language_descriptor[] = {0x09, 0x04};
static const char *string_descriptors[] = {
  language_descriptor,
  "SwitchPad",
  "SwitchPad Four Interface Test",
  "SP4HID01",
  "Controller 1",
  "Controller 2",
  "Controller 3",
  "Controller 4",
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + kControllerCount * TUD_HID_INOUT_DESC_LEN)
static const uint8_t configuration_descriptor[] = {
  TUD_CONFIG_DESCRIPTOR(1, kControllerCount, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 250),
  TUD_HID_INOUT_DESCRIPTOR(0, 4, HID_ITF_PROTOCOL_NONE, sizeof(hid_report_descriptor), 0x01, 0x81, 64, 1),
  TUD_HID_INOUT_DESCRIPTOR(1, 5, HID_ITF_PROTOCOL_NONE, sizeof(hid_report_descriptor), 0x02, 0x82, 64, 1),
  TUD_HID_INOUT_DESCRIPTOR(2, 6, HID_ITF_PROTOCOL_NONE, sizeof(hid_report_descriptor), 0x03, 0x83, 64, 1),
  TUD_HID_INOUT_DESCRIPTOR(3, 7, HID_ITF_PROTOCOL_NONE, sizeof(hid_report_descriptor), 0x04, 0x84, 64, 1),
};

extern "C" uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
  return instance < kControllerCount ? hid_report_descriptor : nullptr;
}

extern "C" uint16_t tud_hid_get_report_cb(uint8_t, uint8_t, hid_report_type_t, uint8_t *, uint16_t) {
  return 0;
}

extern "C" void tud_hid_set_report_cb(uint8_t, uint8_t, hid_report_type_t, uint8_t const *, uint16_t) {}

static esp_err_t root_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static uint32_t now_ms() {
  return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static bool valid_device_id(const char *device_id) {
  const size_t length = strlen(device_id);
  if (length < 8 || length >= sizeof(clients[0].device_id)) return false;
  for (size_t i = 0; i < length; i++) {
    const char c = device_id[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
  }
  return true;
}

static void neutralize(uint8_t player) {
  portENTER_CRITICAL(&report_lock);
  reports[player] = {0, 8, 128, 128, 128, 128, 0};
  last_input_ms[player] = 0;
  portEXIT_CRITICAL(&report_lock);
}

static int claim_slot(const char *device_id) {
  if (!valid_device_id(device_id)) return -1;
  const uint32_t now = now_ms();
  int available = -1;
  for (uint8_t i = 0; i < kControllerCount; i++) {
    if (strcmp(clients[i].device_id, device_id) == 0) {
      clients[i].last_seen_ms = now;
      return i;
    }
    if (available < 0 && (clients[i].device_id[0] == '\0' || now - clients[i].last_seen_ms > kClientLeaseMs)) available = i;
  }
  if (available < 0) return -1;
  neutralize(available);
  strlcpy(clients[available].device_id, device_id, sizeof(clients[available].device_id));
  clients[available].last_seen_ms = now;
  return available;
}

static bool body_value(const char *body, const char *key, char *value, size_t value_size) {
  const size_t key_length = strlen(key);
  const char *cursor = body;
  while ((cursor = strstr(cursor, key)) != nullptr) {
    if ((cursor == body || cursor[-1] == '&') && cursor[key_length] == '=') {
      cursor += key_length + 1;
      const char *end = strchr(cursor, '&');
      const size_t length = end ? static_cast<size_t>(end - cursor) : strlen(cursor);
      if (length >= value_size) return false;
      memcpy(value, cursor, length);
      value[length] = '\0';
      return true;
    }
    cursor += key_length;
  }
  return false;
}

static bool read_body(httpd_req_t *req, char *body, size_t capacity) {
  if (req->content_len <= 0 || static_cast<size_t>(req->content_len) >= capacity) return false;
  size_t received = 0;
  while (received < static_cast<size_t>(req->content_len)) {
    const int result = httpd_req_recv(req, body + received, req->content_len - received);
    if (result == HTTPD_SOCK_ERR_TIMEOUT) continue;
    if (result <= 0) return false;
    received += result;
  }
  body[received] = '\0';
  return true;
}

static int apply_input(const char *body) {
  char device_id[40] = {};
  if (!body_value(body, "device", device_id, sizeof(device_id)) || !valid_device_id(device_id)) return -2;
  const int player = claim_slot(device_id);
  if (player < 0) return -1;

  char value[16] = {};
  portENTER_CRITICAL(&report_lock);
  SwitchReport next = reports[player];
  portEXIT_CRITICAL(&report_lock);
  if (body_value(body, "buttons", value, sizeof(value))) next.buttons = static_cast<uint16_t>(strtoul(value, nullptr, 0));
  if (body_value(body, "hat", value, sizeof(value))) next.hat = static_cast<uint8_t>(strtoul(value, nullptr, 0));
  if (body_value(body, "lx", value, sizeof(value))) next.lx = static_cast<uint8_t>(strtoul(value, nullptr, 0));
  if (body_value(body, "ly", value, sizeof(value))) next.ly = static_cast<uint8_t>(strtoul(value, nullptr, 0));
  if (body_value(body, "rx", value, sizeof(value))) next.rx = static_cast<uint8_t>(strtoul(value, nullptr, 0));
  if (body_value(body, "ry", value, sizeof(value))) next.ry = static_cast<uint8_t>(strtoul(value, nullptr, 0));
  if (next.hat > 8) next.hat = 8;
  next.vendor = 0;
  portENTER_CRITICAL(&report_lock);
  reports[player] = next;
  last_input_ms[player] = now_ms();
  portEXIT_CRITICAL(&report_lock);
  return player;
}

static esp_err_t claim_handler(httpd_req_t *req) {
  char query[96] = {};
  char device_id[40] = {};
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
      httpd_query_key_value(query, "device", device_id, sizeof(device_id)) != ESP_OK || !valid_device_id(device_id)) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid device id");
  }
  const int player = claim_slot(device_id);
  if (player < 0) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    return httpd_resp_sendstr(req, "four controllers in use");
  }
  char response[48] = {};
  snprintf(response, sizeof(response), "{\"player\":%d,\"controllers\":4}", player);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_sendstr(req, response);
}

static esp_err_t input_handler(httpd_req_t *req) {
  char body[256] = {};
  if (!read_body(req, body, sizeof(body))) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid input");
  const int player = apply_input(body);
  if (player == -2) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid device id");
  if (player < 0) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    return httpd_resp_sendstr(req, "four controllers in use");
  }
  return httpd_resp_send(req, nullptr, 0);
}

static esp_err_t websocket_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    char query[96] = {};
    char device_id[40] = {};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "device", device_id, sizeof(device_id)) != ESP_OK || claim_slot(device_id) < 0) {
      return ESP_FAIL;
    }
    return ESP_OK;
  }
  httpd_ws_frame_t frame = {};
  frame.type = HTTPD_WS_TYPE_TEXT;
  if (httpd_ws_recv_frame(req, &frame, 0) != ESP_OK || frame.len >= 256) return ESP_FAIL;
  char body[256] = {};
  frame.payload = reinterpret_cast<uint8_t *>(body);
  if (httpd_ws_recv_frame(req, &frame, frame.len) != ESP_OK) return ESP_FAIL;
  body[frame.len] = '\0';
  apply_input(body);
  return ESP_OK;
}

static void reboot_task(void *) {
  vTaskDelay(pdMS_TO_TICKS(500));
  esp_restart();
}

static esp_err_t update_handler(httpd_req_t *req) {
  char update_key[32] = {};
  if (httpd_req_get_hdr_value_str(req, "X-SwitchPad-Update", update_key, sizeof(update_key)) != ESP_OK ||
      strcmp(update_key, "local-firmware") != 0) {
    return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "update key required");
  }

  const esp_partition_t *partition = esp_ota_get_next_update_partition(nullptr);
  if (!partition || req->content_len <= 0 || static_cast<size_t>(req->content_len) > partition->size) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid firmware size");
  }

  for (uint8_t i = 0; i < kControllerCount; i++) neutralize(i);
  esp_ota_handle_t update_handle = 0;
  esp_err_t result = esp_ota_begin(partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
  if (result != ESP_OK) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "could not start update");

  uint8_t *buffer = static_cast<uint8_t *>(malloc(4096));
  if (!buffer) {
    esp_ota_abort(update_handle);
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
  }

  size_t remaining = req->content_len;
  while (remaining > 0) {
    const size_t requested = remaining < 4096 ? remaining : 4096;
    const int received = httpd_req_recv(req, reinterpret_cast<char *>(buffer), requested);
    if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
    if (received <= 0 || esp_ota_write(update_handle, buffer, received) != ESP_OK) {
      free(buffer);
      esp_ota_abort(update_handle);
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "firmware transfer failed");
    }
    remaining -= received;
  }
  free(buffer);

  result = esp_ota_end(update_handle);
  if (result != ESP_OK || esp_ota_set_boot_partition(partition) != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "firmware verification failed");
  }

  const esp_err_t response = httpd_resp_sendstr(req, "update accepted; rebooting");
  xTaskCreate(reboot_task, "ota_reboot", 2048, nullptr, 8, nullptr);
  return response;
}

static void start_http_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.stack_size = 8192;
  config.max_uri_handlers = 8;
  httpd_handle_t server = nullptr;
  ESP_ERROR_CHECK(httpd_start(&server, &config));
  const httpd_uri_t root_uri = {.uri = "/", .method = HTTP_GET, .handler = root_handler, .user_ctx = nullptr};
  const httpd_uri_t claim_uri = {.uri = "/api/claim", .method = HTTP_GET, .handler = claim_handler, .user_ctx = nullptr};
  const httpd_uri_t input_uri = {.uri = "/api/input", .method = HTTP_POST, .handler = input_handler, .user_ctx = nullptr};
  const httpd_uri_t update_uri = {.uri = "/api/update", .method = HTTP_POST, .handler = update_handler, .user_ctx = nullptr};
  const httpd_uri_t websocket_uri = {
    .uri = "/ws", .method = HTTP_GET, .handler = websocket_handler, .user_ctx = nullptr,
    .is_websocket = true, .handle_ws_control_frames = false, .supported_subprotocol = nullptr,
  };
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_uri));
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &claim_uri));
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &input_uri));
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &update_uri));
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &websocket_uri));
}

static void connect_wifi() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t *netif = esp_netif_create_default_wifi_sta();
  esp_netif_dhcpc_stop(netif);
  esp_netif_ip_info_t ip_info = {};
  ip_info.ip.addr = ipaddr_addr("192.168.0.107");
  ip_info.gw.addr = ipaddr_addr("192.168.0.1");
  ip_info.netmask.addr = ipaddr_addr("255.255.255.0");
  ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  wifi_config_t wifi_config = {};
  strlcpy(reinterpret_cast<char *>(wifi_config.sta.ssid), WIFI_SSID, sizeof(wifi_config.sta.ssid));
  strlcpy(reinterpret_cast<char *>(wifi_config.sta.password), WIFI_PASSWORD, sizeof(wifi_config.sta.password));
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_connect());
}

static void usb_report_task(void *) {
  uint8_t player = 0;
  while (true) {
    const uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    portENTER_CRITICAL(&report_lock);
    if (now - last_input_ms[player] > kInputTimeoutMs) reports[player] = {0, 8, 128, 128, 128, 128, 0};
    SwitchReport report = reports[player];
    portEXIT_CRITICAL(&report_lock);
    if (tud_mounted() && tud_hid_n_ready(player)) tud_hid_n_report(player, 0, &report, sizeof(report));
    player = (player + 1) % kControllerCount;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

extern "C" void app_main() {
  ESP_ERROR_CHECK(nvs_flash_init());
  for (uint8_t i = 0; i < kControllerCount; i++) {
    reports[i] = {0, 8, 128, 128, 128, 128, 0};
    last_input_ms[i] = 0;
  }
  connect_wifi();
  start_http_server();
  tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
  tusb_cfg.descriptor.device = &device_descriptor;
  tusb_cfg.descriptor.string = string_descriptors;
  tusb_cfg.descriptor.string_count = sizeof(string_descriptors) / sizeof(string_descriptors[0]);
  tusb_cfg.descriptor.full_speed_config = configuration_descriptor;
  ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
  xTaskCreate(usb_report_task, "usb_reports", 4096, nullptr, 6, nullptr);
  ESP_LOGI(kTag, "Four HID interfaces ready at http://192.168.0.107/");
}
