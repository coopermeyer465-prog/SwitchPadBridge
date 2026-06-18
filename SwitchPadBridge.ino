#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include "USB.h"
#include "USBHID.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha1.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#endif

static const uint16_t HTTP_PORT = 80;
static const uint16_t UDP_PORT = 7777;
static const uint16_t WS_PORT = 81;
static const uint32_t REPORT_INTERVAL_US = 5000;
static const uint32_t INPUT_TIMEOUT_MS = 800;
static const uint32_t WS_READ_TIMEOUT_MS = 40;

enum SwitchButton : uint16_t {
  SW_Y = 0x0001,
  SW_B = 0x0002,
  SW_A = 0x0004,
  SW_X = 0x0008,
  SW_L = 0x0010,
  SW_R = 0x0020,
  SW_ZL = 0x0040,
  SW_ZR = 0x0080,
  SW_MINUS = 0x0100,
  SW_PLUS = 0x0200,
  SW_LCLICK = 0x0400,
  SW_RCLICK = 0x0800,
  SW_HOME = 0x1000,
  SW_CAPTURE = 0x2000,
};

struct SwitchReport {
  uint16_t buttons;
  uint8_t hat;
  uint8_t lx;
  uint8_t ly;
  uint8_t rx;
  uint8_t ry;
  uint8_t vendor;
} __attribute__((packed));

class SwitchHID_ : public USBHIDDevice {
public:
  SwitchHID_() {
    hid.addDevice(this, sizeof(reportDescriptor));
  }

  void begin() {
    hid.begin();
  }

  bool send(const SwitchReport &report) {
    return hid.SendReport(0, &report, sizeof(report), 2);
  }

  uint16_t _onGetDescriptor(uint8_t *buffer) override {
    memcpy(buffer, reportDescriptor, sizeof(reportDescriptor));
    return sizeof(reportDescriptor);
  }

  void _onOutput(uint8_t report_id, const uint8_t *buffer, uint16_t len) override {
    (void)report_id;
    (void)buffer;
    (void)len;
  }

private:
  USBHID hid;

  static constexpr uint8_t reportDescriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x05,        // Usage (Joystick)
    0xA1, 0x01,        // Collection (Application)
    0x15, 0x00,        // Logical Minimum (0)
    0x25, 0x01,        // Logical Maximum (1)
    0x35, 0x00,        // Physical Minimum (0)
    0x45, 0x01,        // Physical Maximum (1)
    0x75, 0x01,        // Report Size (1)
    0x95, 0x10,        // Report Count (16)
    0x05, 0x09,        // Usage Page (Button)
    0x19, 0x01,        // Usage Minimum (1)
    0x29, 0x10,        // Usage Maximum (16)
    0x81, 0x02,        // Input (Data, Variable, Absolute)
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x25, 0x07,        // Logical Maximum (7)
    0x46, 0x3B, 0x01,  // Physical Maximum (315)
    0x75, 0x04,        // Report Size (4)
    0x95, 0x01,        // Report Count (1)
    0x65, 0x14,        // Unit (English Rotation, degrees)
    0x09, 0x39,        // Usage (Hat switch)
    0x81, 0x42,        // Input (Data, Variable, Absolute, Null State)
    0x65, 0x00,        // Unit (None)
    0x95, 0x01,        // Report Count (1)
    0x81, 0x01,        // Input (Constant)
    0x26, 0xFF, 0x00,  // Logical Maximum (255)
    0x46, 0xFF, 0x00,  // Physical Maximum (255)
    0x09, 0x30,        // Usage (X)
    0x09, 0x31,        // Usage (Y)
    0x09, 0x32,        // Usage (Z)
    0x09, 0x35,        // Usage (Rz)
    0x75, 0x08,        // Report Size (8)
    0x95, 0x04,        // Report Count (4)
    0x81, 0x02,        // Input (Data, Variable, Absolute)
    0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined)
    0x09, 0x20,        // Usage (0x20)
    0x95, 0x01,        // Report Count (1)
    0x81, 0x02,        // Input (Data, Variable, Absolute)
    0x0A, 0x21, 0x26,  // Usage (0x2621)
    0x95, 0x08,        // Report Count (8)
    0x91, 0x02,        // Output (Data, Variable, Absolute)
    0xC0               // End Collection
  };
};

constexpr uint8_t SwitchHID_::reportDescriptor[];

SwitchHID_ SwitchHID;
WebServer server(HTTP_PORT);
WiFiServer wsServer(WS_PORT);
WiFiClient wsClient;
WiFiUDP udp;
Preferences prefs;

SwitchReport currentReport = {0, 8, 128, 128, 128, 128, 0};
uint32_t lastInputMs = 0;
uint32_t lastReportUs = 0;
uint32_t packetsSeen = 0;

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>SwitchPadBridge</title>
<style>
html,body{margin:0;height:100%;background:#101419;color:#f6f8fb;font-family:-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif;touch-action:none;user-select:none}
.wrap{height:100%;display:grid;grid-template-rows:auto 1fr auto;gap:12px;padding:14px;box-sizing:border-box}
.top{display:flex;align-items:center;justify-content:space-between;font-size:14px;color:#aeb8c4}
.status{width:10px;height:10px;border-radius:50%;background:#d04d4d;display:inline-block;margin-right:8px}
.status.ok{background:#30d47a}
.pad{display:grid;grid-template-columns:1fr 1fr;gap:16px;align-items:center}
.cluster{position:relative;aspect-ratio:1;min-height:220px}
button{position:absolute;border:0;border-radius:999px;background:#27313d;color:#fff;font-weight:700;font-size:20px;box-shadow:inset 0 0 0 1px #3c4652}
button:active,button.down{background:#2f8cff}
.face{width:72px;height:72px}
.small{width:58px;height:42px;font-size:13px;border-radius:10px}
.stick{position:absolute;left:50%;top:50%;width:166px;height:166px;border-radius:50%;transform:translate(-50%,-50%);background:#171d25;box-shadow:inset 0 0 0 2px #33404d}
.nub{position:absolute;left:50%;top:50%;width:72px;height:72px;border-radius:50%;transform:translate(-50%,-50%);background:#d7dee8}
.foot{display:grid;grid-template-columns:repeat(6,1fr);gap:8px}
.foot button{position:static;width:100%;height:44px}
#bY{left:12%;top:38%}#bA{right:12%;top:38%}#bX{left:50%;top:8%;transform:translateX(-50%)}#bB{left:50%;bottom:8%;transform:translateX(-50%)}
#bL{left:2%;top:0}#bR{right:2%;top:0}#bZL{left:2%;top:52px}#bZR{right:2%;top:52px}
@media(max-width:720px){.pad{grid-template-columns:1fr}.cluster{min-height:190px}.face{width:64px;height:64px}.foot{grid-template-columns:repeat(3,1fr)}}
</style>
</head>
<body>
<div class="wrap">
  <div class="top"><div><span id="dot" class="status"></span><span id="status">connecting</span></div><div id="gp">touch</div></div>
  <div class="pad">
    <div class="cluster">
      <div id="stickL" class="stick"><div class="nub"></div></div>
      <button id="bL" class="small" data-bit="16">L</button>
      <button id="bZL" class="small" data-bit="64">ZL</button>
    </div>
    <div class="cluster">
      <button id="bY" class="face" data-bit="1">Y</button>
      <button id="bA" class="face" data-bit="4">A</button>
      <button id="bX" class="face" data-bit="8">X</button>
      <button id="bB" class="face" data-bit="2">B</button>
      <button id="bR" class="small" data-bit="32">R</button>
      <button id="bZR" class="small" data-bit="128">ZR</button>
    </div>
  </div>
  <div class="foot">
    <button data-bit="256">-</button><button data-bit="512">+</button><button data-bit="4096">Home</button>
    <button data-bit="8192">Cap</button><button data-bit="1024">L3</button><button data-bit="2048">R3</button>
  </div>
</div>
<script>
const state={buttons:0,hat:8,lx:128,ly:128,rx:128,ry:128};
let ws, connected=false, lastSent="";
const dot=document.getElementById("dot"), statusEl=document.getElementById("status"), gpEl=document.getElementById("gp");
function connect(){
  ws=new WebSocket(`ws://${location.hostname}:81/ws`);
  ws.onopen=()=>{connected=true;dot.classList.add("ok");statusEl.textContent="connected"};
  ws.onclose=()=>{connected=false;dot.classList.remove("ok");statusEl.textContent="reconnecting";setTimeout(connect,700)};
}
connect();
function payload(){return `buttons=${state.buttons}&hat=${state.hat}&lx=${state.lx}&ly=${state.ly}&rx=${state.rx}&ry=${state.ry}`}
function send(){const p=payload();if(p!==lastSent&&connected){ws.send(p);lastSent=p}}
for(const b of document.querySelectorAll("button[data-bit]")){
  const bit=Number(b.dataset.bit);
  const on=e=>{e.preventDefault();state.buttons|=bit;b.classList.add("down");send()};
  const off=e=>{e.preventDefault();state.buttons&=~bit;b.classList.remove("down");send()};
  b.addEventListener("pointerdown",on); b.addEventListener("pointerup",off);
  b.addEventListener("pointercancel",off); b.addEventListener("pointerleave",off);
}
const stick=document.getElementById("stickL"), nub=stick.querySelector(".nub");
let stickId=null;
function setStick(e){
  const r=stick.getBoundingClientRect(), cx=r.left+r.width/2, cy=r.top+r.height/2, max=r.width*0.34;
  let dx=e.clientX-cx, dy=e.clientY-cy, d=Math.hypot(dx,dy);
  if(d>max){dx=dx/d*max;dy=dy/d*max}
  nub.style.transform=`translate(calc(-50% + ${dx}px),calc(-50% + ${dy}px))`;
  state.lx=Math.round(128+dx/max*127); state.ly=Math.round(128+dy/max*127); send();
}
stick.addEventListener("pointerdown",e=>{stickId=e.pointerId;stick.setPointerCapture(stickId);setStick(e)});
stick.addEventListener("pointermove",e=>{if(e.pointerId===stickId)setStick(e)});
function centerStick(){stickId=null;nub.style.transform="translate(-50%,-50%)";state.lx=128;state.ly=128;send()}
stick.addEventListener("pointerup",centerStick); stick.addEventListener("pointercancel",centerStick);
function pollGamepad(){
  const gps=navigator.getGamepads?navigator.getGamepads():[];
  const g=[...gps].find(Boolean);
  if(g){
    gpEl.textContent=g.id.slice(0,24);
    let b=0;
    const map=[2,1,0,3,4,5,6,7,8,9,10,11,16,17];
    const bits=[2,4,1,8,16,32,64,128,256,512,1024,2048,4096,8192];
    map.forEach((idx,i)=>{if(g.buttons[idx]?.pressed)b|=bits[i]});
    state.buttons=b;
    state.lx=Math.round((g.axes[0]||0)*127+128); state.ly=Math.round((g.axes[1]||0)*127+128);
    state.rx=Math.round((g.axes[2]||0)*127+128); state.ry=Math.round((g.axes[3]||0)*127+128);
    state.hat=8;
    if(g.buttons[12]?.pressed)state.hat=0; else if(g.buttons[13]?.pressed)state.hat=4;
    else if(g.buttons[14]?.pressed)state.hat=6; else if(g.buttons[15]?.pressed)state.hat=2;
    send();
  }
  requestAnimationFrame(pollGamepad);
}
pollGamepad();
setInterval(send,16);
</script>
</body>
</html>
)HTML";

const char SETUP_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SwitchPadBridge Setup</title>
<style>
html,body{margin:0;min-height:100%;background:#101419;color:#f6f8fb;font-family:-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif}
main{max-width:420px;margin:0 auto;padding:28px 18px}
h1{font-size:24px;margin:0 0 18px}
label{display:block;margin:14px 0 6px;color:#aeb8c4}
input,button{box-sizing:border-box;width:100%;height:46px;border-radius:8px;border:1px solid #3c4652;background:#171d25;color:#fff;font-size:16px;padding:0 12px}
button{margin-top:18px;background:#2f8cff;border:0;font-weight:700}
p{line-height:1.45;color:#aeb8c4}
</style>
</head>
<body>
<main>
<h1>SwitchPadBridge Setup</h1>
<p>Enter your home Wi-Fi. The ESP32 will save it, reboot, and join that network.</p>
<form method="post" action="/save">
<label>Wi-Fi name</label>
<input name="ssid" autocomplete="off" required>
<label>Password</label>
<input name="password" type="password">
<button>Save and reboot</button>
</form>
</main>
</body>
</html>
)HTML";

String getParam(const String &body, const char *key) {
  String needle = String(key) + "=";
  int start = body.indexOf(needle);
  if (start < 0) {
    needle = String("\"") + key + "\":";
    start = body.indexOf(needle);
  }
  if (start < 0) {
    return "";
  }
  start += needle.length();
  while (start < (int)body.length() && (body[start] == ' ' || body[start] == '"')) start++;
  int end = start;
  while (end < (int)body.length() && body[end] != '&' && body[end] != ',' && body[end] != '}' && body[end] != '"') end++;
  return body.substring(start, end);
}

uint32_t parseNumber(const String &value, uint32_t fallback) {
  if (!value.length()) return fallback;
  char *end = nullptr;
  return strtoul(value.c_str(), &end, 0);
}

uint8_t clampByte(uint32_t value) {
  return value > 255 ? 255 : (uint8_t)value;
}

void applyInput(const String &body) {
  noInterrupts();
  SwitchReport next = currentReport;
  interrupts();

  next.buttons = (uint16_t)parseNumber(getParam(body, "buttons"), next.buttons);
  next.hat = (uint8_t)parseNumber(getParam(body, "hat"), next.hat);
  next.lx = clampByte(parseNumber(getParam(body, "lx"), next.lx));
  next.ly = clampByte(parseNumber(getParam(body, "ly"), next.ly));
  next.rx = clampByte(parseNumber(getParam(body, "rx"), next.rx));
  next.ry = clampByte(parseNumber(getParam(body, "ry"), next.ry));
  if (next.hat > 8) next.hat = 8;
  next.vendor = 0;

  noInterrupts();
  currentReport = next;
  interrupts();
  lastInputMs = millis();
  packetsSeen++;
}

String stateJson() {
  noInterrupts();
  SwitchReport r = currentReport;
  interrupts();
  String out = "{";
  out += "\"buttons\":" + String(r.buttons) + ",";
  out += "\"hat\":" + String(r.hat) + ",";
  out += "\"lx\":" + String(r.lx) + ",";
  out += "\"ly\":" + String(r.ly) + ",";
  out += "\"rx\":" + String(r.rx) + ",";
  out += "\"ry\":" + String(r.ry) + ",";
  out += "\"packets\":" + String(packetsSeen) + ",";
  out += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  out += "}";
  return out;
}

void setupHttp() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });
  server.on("/api/state", HTTP_GET, []() {
    server.send(200, "application/json", stateJson());
  });
  server.on("/health", HTTP_GET, []() {
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/input", HTTP_POST, []() {
    applyInput(server.arg("plain"));
    server.send(204, "text/plain", "");
  });
  server.onNotFound([]() {
    server.send(404, "text/plain", "not found");
  });
  server.begin();
}

void setupConfigPortal() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", SETUP_HTML);
  });
  server.on("/save", HTTP_POST, []() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("password", password);
    prefs.end();
    server.send(200, "text/html", "<!doctype html><meta name='viewport' content='width=device-width,initial-scale=1'><body style='font-family:sans-serif;background:#101419;color:white;padding:24px'>Saved. Rebooting...</body>");
    delay(700);
    ESP.restart();
  });
  server.begin();
}

String wsAcceptKey(const String &key) {
  const char *guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  String joined = key + guid;
  uint8_t sha[20];
  mbedtls_sha1((const unsigned char *)joined.c_str(), joined.length(), sha);
  unsigned char encoded[32] = {0};
  size_t outLen = 0;
  mbedtls_base64_encode(encoded, sizeof(encoded), &outLen, sha, sizeof(sha));
  encoded[min(outLen, sizeof(encoded) - 1)] = 0;
  return String((const char *)encoded);
}

void handleWsHandshake(WiFiClient &client) {
  String req = "";
  uint32_t start = millis();
  while (client.connected() && millis() - start < 1000) {
    while (client.available()) {
      char c = client.read();
      req += c;
      if (req.endsWith("\r\n\r\n")) {
        int k = req.indexOf("Sec-WebSocket-Key:");
        if (k < 0) {
          client.stop();
          return;
        }
        k += 18;
        while (k < (int)req.length() && req[k] == ' ') k++;
        int e = req.indexOf("\r\n", k);
        String key = req.substring(k, e);
        String accept = wsAcceptKey(key);
        client.print("HTTP/1.1 101 Switching Protocols\r\n");
        client.print("Upgrade: websocket\r\n");
        client.print("Connection: Upgrade\r\n");
        client.print("Sec-WebSocket-Accept: ");
        client.print(accept);
        client.print("\r\n\r\n");
        return;
      }
    }
    delay(1);
  }
  client.stop();
}

bool waitForWsBytes(size_t count) {
  uint32_t start = millis();
  while (wsClient.connected() && wsClient.available() < (int)count) {
    if (millis() - start >= WS_READ_TIMEOUT_MS) return false;
    delay(1);
  }
  return wsClient.connected() && wsClient.available() >= (int)count;
}

void sendWsControlFrame(uint8_t opcode, const uint8_t *payload, size_t len) {
  if (!wsClient.connected() || len > 125) return;
  uint8_t header[2] = {(uint8_t)(0x80 | opcode), (uint8_t)len};
  wsClient.write(header, sizeof(header));
  if (len) wsClient.write(payload, len);
}

void pollWebSocket() {
  WiFiClient next = wsServer.available();
  if (next) {
    if (wsClient && wsClient.connected()) wsClient.stop();
    wsClient = next;
    handleWsHandshake(wsClient);
  }

  if (!wsClient || !wsClient.connected() || wsClient.available() < 2) return;

  uint8_t b0 = wsClient.read();
  uint8_t b1 = wsClient.read();
  uint8_t opcode = b0 & 0x0F;
  bool masked = b1 & 0x80;
  uint64_t len = b1 & 0x7F;

  if (len == 126) {
    if (!waitForWsBytes(2)) {
      wsClient.stop();
      return;
    }
    len = ((uint16_t)wsClient.read() << 8) | wsClient.read();
  } else if (len == 127) {
    wsClient.stop();
    return;
  }

  if (!masked || len > 512) {
    wsClient.stop();
    return;
  }

  uint8_t mask[4] = {0, 0, 0, 0};
  if (!waitForWsBytes(4)) {
    wsClient.stop();
    return;
  }
  for (int i = 0; i < 4; i++) mask[i] = wsClient.read();

  String msg = "";
  msg.reserve((size_t)len);
  for (uint64_t i = 0; i < len; i++) {
    if (!waitForWsBytes(1)) {
      wsClient.stop();
      return;
    }
    char c = wsClient.read();
    c ^= mask[i & 3];
    msg += c;
  }

  if (opcode == 0x8) {
    wsClient.stop();
  } else if (opcode == 0x9) {
    sendWsControlFrame(0xA, (const uint8_t *)msg.c_str(), msg.length());
  } else if (opcode == 0x1 && msg.length()) {
    applyInput(msg);
  }
}

void pollUdp() {
  int packetSize = udp.parsePacket();
  if (packetSize <= 0) return;
  char buf[256];
  int len = udp.read(buf, sizeof(buf) - 1);
  if (len <= 0) return;
  buf[len] = 0;
  applyInput(String(buf));
}

bool connectWifi() {
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String password = prefs.getString("password", "");
  prefs.end();

  if (!ssid.length()) {
    ssid = WIFI_SSID;
    password = WIFI_PASSWORD;
  }

  if (!ssid.length()) return false;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Joining Wi-Fi");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    Serial.print(".");
    delay(400);
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi join failed.");
    return false;
  }
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  if (MDNS.begin("switchpad")) {
    MDNS.addService("http", "tcp", HTTP_PORT);
    Serial.println("mDNS: http://switchpad.local/");
  } else {
    Serial.println("mDNS start failed.");
  }
  return true;
}

void startSetupAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("SwitchPadBridge-Setup");
  Serial.println("No saved Wi-Fi. Started setup hotspot:");
  Serial.println("SSID: SwitchPadBridge-Setup");
  Serial.print("Setup URL: http://");
  Serial.println(WiFi.softAPIP());
  setupConfigPortal();
  while (true) {
    server.handleClient();
    delay(2);
  }
}

void setupUsb() {
  USB.VID(0x0F0D);
  USB.PID(0x0092);
  USB.manufacturerName("HORI CO.,LTD.");
  USB.productName("POKKEN CONTROLLER");
  USB.usbPower(500);
  SwitchHID.begin();
  USB.begin();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  if (!connectWifi()) {
    startSetupAccessPoint();
  }
  setupHttp();
  wsServer.begin();
  udp.begin(UDP_PORT);
  setupUsb();
  lastInputMs = millis();
  Serial.println("Open the controller page on your phone:");
  Serial.print("http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
  pollWebSocket();
  pollUdp();

  if (millis() - lastInputMs > INPUT_TIMEOUT_MS) {
    noInterrupts();
    currentReport = {0, 8, 128, 128, 128, 128, 0};
    interrupts();
  }

  uint32_t now = micros();
  if (now - lastReportUs >= REPORT_INTERVAL_US) {
    lastReportUs = now;
    noInterrupts();
    SwitchReport report = currentReport;
    interrupts();
    SwitchHID.send(report);
  }
}
