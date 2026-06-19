#include <cstring>
#include <cstdlib>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/ip_addr.h"
#include "nvs_flash.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"
#include "secrets.h"

static constexpr uint8_t kControllerCount = 4;
static constexpr uint32_t kInputTimeoutMs = 900;
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

static const char diagnostic_page[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>SwitchPad Four HID Test</title><style>
*{box-sizing:border-box}body{margin:0;background:#0b0e12;color:#f5f7fa;font-family:-apple-system,sans-serif;padding:18px;touch-action:manipulation}main{max-width:720px;margin:auto}h1{font-size:22px;margin:0 0 8px}p{color:#9eabb8;margin:0 0 18px}.players{display:grid;grid-template-columns:1fr 1fr;gap:12px}.player{border:1px solid #46515d;border-radius:8px;padding:12px;background:#151a20}.player h2{font-size:16px;margin:0 0 10px;color:#64c7ff}.buttons{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}button{min-height:48px;border:1px solid #596673;border-radius:7px;background:#282f37;color:white;font:700 15px inherit;touch-action:none}button.down{background:#176b69;border-color:#36ddd5}.pair{grid-column:span 2}@media(max-width:560px){.players{grid-template-columns:1fr}}</style></head>
<body><main><h1>Four-interface test</h1><p>Open Change Grip/Order, then hold L+R for each player. Each panel sends to a different USB HID interface.</p><div class="players" id="players"></div></main>
<script>
const actions=[['L + R',48,'pair'],['A',4,''],['B',2,''],['X',8,''],['Y',1,'']];
const root=document.getElementById('players');
for(let p=0;p<4;p++){const box=document.createElement('section');box.className='player';box.innerHTML=`<h2>Player ${p+1}</h2><div class="buttons">${actions.map(([n,b,c])=>`<button class="${c}" data-player="${p}" data-buttons="${b}">${n}</button>`).join('')}</div>`;root.appendChild(box)}
let timer=0;async function send(p,b){fetch(`/input?player=${p}&buttons=${b}`,{cache:'no-store'}).catch(()=>{})}
for(const b of document.querySelectorAll('button')){const start=e=>{e.preventDefault();b.setPointerCapture(e.pointerId);b.classList.add('down');send(b.dataset.player,b.dataset.buttons);clearInterval(timer);timer=setInterval(()=>send(b.dataset.player,b.dataset.buttons),120)};const end=e=>{e.preventDefault();clearInterval(timer);timer=0;b.classList.remove('down');send(b.dataset.player,0)};b.addEventListener('pointerdown',start);b.addEventListener('pointerup',end);b.addEventListener('pointercancel',end);b.addEventListener('lostpointercapture',end)}
</script></body></html>)HTML";

static esp_err_t root_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, diagnostic_page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t input_handler(httpd_req_t *req) {
  char query[96] = {};
  char value[16] = {};
  uint8_t player = 0;
  uint16_t buttons = 0;
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    if (httpd_query_key_value(query, "player", value, sizeof(value)) == ESP_OK) player = static_cast<uint8_t>(atoi(value));
    if (httpd_query_key_value(query, "buttons", value, sizeof(value)) == ESP_OK) buttons = static_cast<uint16_t>(strtoul(value, nullptr, 0));
  }
  if (player < kControllerCount) {
    portENTER_CRITICAL(&report_lock);
    reports[player].buttons = buttons;
    last_input_ms[player] = xTaskGetTickCount() * portTICK_PERIOD_MS;
    portEXIT_CRITICAL(&report_lock);
  }
  return httpd_resp_sendstr(req, "ok");
}

static void start_http_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.stack_size = 6144;
  httpd_handle_t server = nullptr;
  ESP_ERROR_CHECK(httpd_start(&server, &config));
  const httpd_uri_t root_uri = {.uri = "/", .method = HTTP_GET, .handler = root_handler, .user_ctx = nullptr};
  const httpd_uri_t input_uri = {.uri = "/input", .method = HTTP_GET, .handler = input_handler, .user_ctx = nullptr};
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_uri));
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &input_uri));
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
