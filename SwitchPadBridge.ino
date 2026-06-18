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
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name="theme-color" content="#0b0e12">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<title>SwitchPadBridge</title>
<style>
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;width:100%;height:100%;overflow:hidden;background:#0b0e12;color:#f5f7fa;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;touch-action:none;user-select:none;-webkit-user-select:none}
button{font:inherit;color:inherit;touch-action:none;cursor:pointer}
.gamepad{position:fixed;inset:0;width:100vw;height:100vh;height:100dvh;overflow:hidden;background:#0b0e12}
.side{position:absolute;top:0;bottom:0;width:42%;min-width:300px}
.left{left:0}.right{right:0}
.utility{position:absolute;z-index:5;top:max(14px,env(safe-area-inset-top));left:50%;display:flex;gap:clamp(8px,1.4vw,18px);transform:translateX(-50%)}
.utility button,.shoulders button{border:1px solid #4b5561;background:#242a31;box-shadow:0 5px 12px #0008,inset 0 1px #ffffff12;font-weight:750}
.utility button{position:relative;width:clamp(44px,5.2vw,62px);height:clamp(38px,5vw,54px);border-radius:7px;font-size:clamp(14px,1.5vw,18px)}
.shoulders{position:absolute;z-index:3;top:max(14px,env(safe-area-inset-top));display:flex;gap:clamp(8px,1.3vw,16px)}
.left .shoulders{left:max(14px,env(safe-area-inset-left))}.right .shoulders{right:max(14px,env(safe-area-inset-right))}
.shoulders button{position:relative;width:clamp(70px,9vw,132px);height:clamp(42px,5.3vw,60px);border-radius:7px;font-size:clamp(14px,1.7vw,20px)}
.control{position:absolute}
.stick{width:clamp(112px,15vw,174px);aspect-ratio:1;border-radius:50%;background:#151a20;border:2px solid #49515b;box-shadow:0 8px 20px #0009,inset 0 0 0 10px #222830;touch-action:none}
.stick:after{content:"";position:absolute;inset:19%;border:1px solid #555e68;border-radius:50%}
.nub{position:absolute;z-index:2;left:50%;top:50%;width:45%;aspect-ratio:1;border-radius:50%;transform:translate(-50%,-50%);background:#343b44;border:2px solid #626b76;box-shadow:0 5px 9px #0008}
#stickL{left:max(5vw,env(safe-area-inset-left));top:27%}#stickR{right:max(6vw,env(safe-area-inset-right));bottom:7%}
.face{right:max(4.5vw,env(safe-area-inset-right));top:25%;width:clamp(158px,20vw,226px);aspect-ratio:1}
.face button{position:absolute;width:clamp(54px,6.6vw,76px);aspect-ratio:1;border:2px solid #555e68;border-radius:50%;background:#282e35;box-shadow:0 6px 12px #0009;font-size:clamp(19px,2.4vw,28px);font-weight:800}
#bX{left:50%;top:0;transform:translateX(-50%);color:#4ba9ff}#bB{left:50%;bottom:0;transform:translateX(-50%);color:#ffd83d}#bY{left:0;top:50%;transform:translateY(-50%);color:#43d778}#bA{right:0;top:50%;transform:translateY(-50%);color:#ff625c}
.dpad{left:max(7vw,env(safe-area-inset-left));bottom:7%;width:clamp(132px,17vw,192px);aspect-ratio:1}
.dpad button{position:absolute;width:34%;height:42%;border:1px solid #505862;background:#2a3037;font-size:0}
.dpad button:after{content:"";position:absolute;left:50%;top:50%;width:0;height:0;transform:translate(-50%,-50%);border:8px solid transparent}
.dpad .up{left:33%;top:0;border-radius:7px 7px 2px 2px}.dpad .south{left:33%;bottom:0;border-radius:2px 2px 7px 7px}.dpad .west{left:0;top:29%;width:42%;height:34%;border-radius:7px 2px 2px 7px}.dpad .east{right:0;top:29%;width:42%;height:34%;border-radius:2px 7px 7px 2px}
.dpad .up:after{border-bottom-color:#eef2f6;margin-top:-5px}.dpad .south:after{border-top-color:#eef2f6;margin-top:5px}.dpad .west:after{border-right-color:#eef2f6;margin-left:-5px}.dpad .east:after{border-left-color:#eef2f6;margin-left:5px}
.dpad:after{content:"";position:absolute;pointer-events:none;left:38%;top:38%;width:24%;height:24%;border-radius:50%;background:#242a31;border:1px solid #3e4650}
.statusbar{position:absolute;z-index:6;display:flex;align-items:center;gap:10px;color:#9ca8b5;font-size:12px}
.statusbar.connection{top:max(74px,calc(env(safe-area-inset-top) + 62px));left:50%;transform:translateX(-50%)}
.statusbar.physical{top:auto;bottom:max(10px,env(safe-area-inset-bottom));left:50%;right:auto;transform:translateX(-50%);max-width:34%;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.dot{width:9px;height:9px;flex:none;border-radius:50%;background:#e05555;box-shadow:0 0 0 3px #e0555525}.dot.ok{background:#24d6cf;box-shadow:0 0 0 3px #24d6cf28}
.fullscreen{position:relative;width:42px;height:42px;border:1px solid #4b5561;border-radius:7px;background:#20262d}
.fullscreen i,.fullscreen i:before,.fullscreen i:after{position:absolute;content:"";width:8px;height:8px;border-color:#e9eef4}.fullscreen i{left:9px;top:9px;border-left:2px solid;border-top:2px solid}.fullscreen i:before{left:14px;top:-2px;border-right:2px solid;border-top:2px solid}.fullscreen i:after{left:-2px;top:14px;border-left:2px solid;border-bottom:2px solid}.fullscreen span{position:absolute;right:9px;bottom:9px;width:8px;height:8px;border-right:2px solid;border-bottom:2px solid}
button.down,button:active{background:#3b4652!important;box-shadow:inset 0 2px 7px #000b!important;transform:translateY(1px)}
.face button.down,.face button:active{transform:translateY(-50%) scale(.96)}#bX.down,#bX:active,#bB.down,#bB:active{transform:translateX(-50%) scale(.96)}
@media(orientation:portrait){
 .side{top:76px;width:50%;min-width:0}.utility{top:max(12px,env(safe-area-inset-top));gap:7px}.utility button{width:42px;height:40px;font-size:13px}.statusbar.connection{display:none}
 .shoulders{top:10px;gap:6px}.shoulders button{width:clamp(58px,17vw,92px);height:40px;font-size:13px}.left .shoulders{left:8px}.right .shoulders{right:8px}
 .stick{width:clamp(104px,34vw,164px)}#stickL{left:50%;top:13%;transform:translateX(-50%)}#stickR{right:50%;bottom:7%;transform:translateX(50%)}
 .dpad{left:50%;bottom:8%;width:clamp(122px,38vw,174px);transform:translateX(-50%)}.face{right:50%;top:18%;width:clamp(142px,40vw,188px);transform:translateX(50%)}
 .statusbar.physical{max-width:76%;font-size:11px}
}
@media(orientation:landscape) and (max-height:430px){.statusbar.connection{display:none}.stick{width:clamp(104px,24vh,132px)}.face{width:clamp(146px,40vh,176px)}.dpad{width:clamp(122px,36vh,158px)}#stickL{top:27%}#stickR{bottom:4%}.face{top:25%}.dpad{bottom:4%}.shoulders button{height:38px}.utility button{height:38px}.statusbar.physical{bottom:4px}}
</style>
</head>
<body>
<main class="gamepad">
  <div class="utility">
    <button data-bit="256" aria-label="Minus">-</button>
    <button data-bit="4096" aria-label="Home">HOME</button>
    <button data-bit="512" aria-label="Plus">+</button>
    <button data-bit="8192" aria-label="Capture">CAP</button>
    <button id="fullscreen" class="fullscreen" aria-label="Enter fullscreen"><i></i><span></span></button>
  </div>
  <section class="side left" aria-label="Left controls">
    <div class="shoulders"><button data-bit="64">ZL</button><button data-bit="16">L</button></div>
    <div id="stickL" class="control stick" data-x="lx" data-y="ly" data-click="1024"><div class="nub"></div></div>
    <div class="control dpad">
      <button class="up" data-dir="up" aria-label="Up"></button><button class="south" data-dir="down" aria-label="Down"></button>
      <button class="west" data-dir="left" aria-label="Left"></button><button class="east" data-dir="right" aria-label="Right"></button>
    </div>
  </section>
  <section class="side right" aria-label="Right controls">
    <div class="shoulders"><button data-bit="32">R</button><button data-bit="128">ZR</button></div>
    <div class="control face">
      <button id="bX" data-bit="8">X</button><button id="bB" data-bit="2">B</button>
      <button id="bY" data-bit="1">Y</button><button id="bA" data-bit="4">A</button>
    </div>
    <div id="stickR" class="control stick" data-x="rx" data-y="ry" data-click="2048"><div class="nub"></div></div>
  </section>
  <div class="statusbar connection"><span id="dot" class="dot"></span><span id="status">connecting</span></div>
  <div id="gp" class="statusbar physical">touch controls</div>
</main>
<script>
const neutral=()=>({buttons:0,hat:8,lx:128,ly:128,rx:128,ry:128});
const touch=neutral(), physical=neutral();
const activeSticks={lx:false,rx:false};
const dirs=new Set();
let ws,connected=false,lastSent="",physicalConnected=false;
const dot=document.getElementById("dot"),statusEl=document.getElementById("status"),gpEl=document.getElementById("gp");
function connect(){
  ws=new WebSocket(`ws://${location.hostname}:81/ws`);
  ws.onopen=()=>{connected=true;dot.classList.add("ok");statusEl.textContent="connected";lastSent=""};
  ws.onclose=()=>{connected=false;dot.classList.remove("ok");statusEl.textContent="reconnecting";setTimeout(connect,700)};
}
connect();
function merged(){
  return {buttons:touch.buttons|physical.buttons,hat:touch.hat!==8?touch.hat:physical.hat,
    lx:activeSticks.lx?touch.lx:physicalConnected?physical.lx:touch.lx,
    ly:activeSticks.lx?touch.ly:physicalConnected?physical.ly:touch.ly,
    rx:activeSticks.rx?touch.rx:physicalConnected?physical.rx:touch.rx,
    ry:activeSticks.rx?touch.ry:physicalConnected?physical.ry:touch.ry};
}
function payload(){const s=merged();return `buttons=${s.buttons}&hat=${s.hat}&lx=${s.lx}&ly=${s.ly}&rx=${s.rx}&ry=${s.ry}`}
function send(force=false){const p=payload();if((force||p!==lastSent)&&connected){ws.send(p);lastSent=p}}
function pressButton(el,on){
  const bit=Number(el.dataset.bit);touch.buttons=on?touch.buttons|bit:touch.buttons&~bit;el.classList.toggle("down",on);send();
  if(on&&navigator.vibrate)navigator.vibrate(8);
}
for(const b of document.querySelectorAll("button[data-bit]")){
  b.addEventListener("pointerdown",e=>{e.preventDefault();b.setPointerCapture(e.pointerId);pressButton(b,true)});
  const off=e=>{e.preventDefault();pressButton(b,false)};
  b.addEventListener("pointerup",off);b.addEventListener("pointercancel",off);b.addEventListener("lostpointercapture",()=>pressButton(b,false));
}
function updateHat(){
  const u=dirs.has("up"),d=dirs.has("down"),l=dirs.has("left"),r=dirs.has("right");
  touch.hat=u&&r?1:r&&d?3:d&&l?5:l&&u?7:u?0:r?2:d?4:l?6:8;send();
}
for(const b of document.querySelectorAll("button[data-dir]")){
  b.addEventListener("pointerdown",e=>{e.preventDefault();b.setPointerCapture(e.pointerId);dirs.add(b.dataset.dir);b.classList.add("down");updateHat()});
  const off=e=>{e.preventDefault();dirs.delete(b.dataset.dir);b.classList.remove("down");updateHat()};
  b.addEventListener("pointerup",off);b.addEventListener("pointercancel",off);b.addEventListener("lostpointercapture",off);
}
for(const stick of document.querySelectorAll(".stick")){
  const nub=stick.querySelector(".nub"),x=stick.dataset.x,y=stick.dataset.y,click=Number(stick.dataset.click);let pointer=null,start=0;
  const move=e=>{const r=stick.getBoundingClientRect(),max=r.width*.31;let dx=e.clientX-r.left-r.width/2,dy=e.clientY-r.top-r.height/2,d=Math.hypot(dx,dy);if(d>max){dx=dx/d*max;dy=dy/d*max}nub.style.transform=`translate(calc(-50% + ${dx}px),calc(-50% + ${dy}px))`;touch[x]=Math.round(128+dx/max*127);touch[y]=Math.round(128+dy/max*127);send()};
  stick.addEventListener("pointerdown",e=>{e.preventDefault();pointer=e.pointerId;start=performance.now();activeSticks[x]=true;stick.setPointerCapture(pointer);move(e)});
  stick.addEventListener("pointermove",e=>{if(e.pointerId===pointer)move(e)});
  const end=e=>{if(pointer===null)return;if(performance.now()-start<180){touch.buttons|=click;setTimeout(()=>{touch.buttons&=~click;send()},70)}pointer=null;activeSticks[x]=false;nub.style.transform="translate(-50%,-50%)";touch[x]=128;touch[y]=128;send()};
  stick.addEventListener("pointerup",end);stick.addEventListener("pointercancel",end);stick.addEventListener("lostpointercapture",end);
}
function pressed(g,i){return !!g.buttons[i]&&(g.buttons[i].pressed||g.buttons[i].value>.5)}
function pollGamepads(){
  const pads=navigator.getGamepads?[...navigator.getGamepads()].filter(Boolean):[];physicalConnected=pads.length>0;Object.assign(physical,neutral());
  if(pads.length){
    const names=[];let axesPad=null;
    for(const g of pads){
      names.push(g.id.replace(/\s*\([^)]*vendor[^)]*\)/i,"").slice(0,28));
      const indices=[0,1,2,3,4,5,6,7,8,9,10,11,16,17],bits=[2,4,1,8,16,32,64,128,256,512,1024,2048,4096,8192];
      indices.forEach((idx,i)=>{if(pressed(g,idx))physical.buttons|=bits[i]});
      if(pressed(g,12))physical.hat=0;else if(pressed(g,13))physical.hat=4;else if(pressed(g,14))physical.hat=6;else if(pressed(g,15))physical.hat=2;
      if(!axesPad&&g.axes.length>=2)axesPad=g;
    }
    if(axesPad){const a=axesPad.axes,axis=i=>Math.round(Math.max(-1,Math.min(1,a[i]||0))*127+128);physical.lx=axis(0);physical.ly=axis(1);if(a.length>=4){physical.rx=axis(2);physical.ry=axis(3)}}
    gpEl.textContent=names.join(" + ");
  }else gpEl.textContent="touch controls";
  send();requestAnimationFrame(pollGamepads);
}
window.addEventListener("gamepadconnected",()=>send(true));window.addEventListener("gamepaddisconnected",()=>send(true));pollGamepads();setInterval(()=>send(true),250);
document.getElementById("fullscreen").addEventListener("click",async()=>{
  try{if(!document.fullscreenElement){await document.documentElement.requestFullscreen?.();await screen.orientation?.lock?.("landscape")}else await document.exitFullscreen?.()}catch(e){}
  window.scrollTo(0,1);
});
document.addEventListener("contextmenu",e=>e.preventDefault());
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
