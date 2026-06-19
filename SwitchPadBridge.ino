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
static const uint8_t CONTROLLER_COUNT = 1;
static const uint32_t REPORT_INTERVAL_US = 4000;
static const uint32_t INPUT_TIMEOUT_MS = 800;
static const uint32_t WS_READ_TIMEOUT_MS = 500;
static const IPAddress STATIC_IP(192, 168, 0, 107);
static const IPAddress GATEWAY_IP(192, 168, 0, 1);
static const IPAddress SUBNET_MASK(255, 255, 255, 0);

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

  bool send(uint8_t player, const SwitchReport &report) {
    (void)player;
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

  #define SWITCH_GAMEPAD_REPORT \
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, \
    0x15, 0x00, 0x25, 0x01, 0x35, 0x00, 0x45, 0x01, \
    0x75, 0x01, 0x95, 0x10, 0x05, 0x09, 0x19, 0x01, \
    0x29, 0x10, 0x81, 0x02, 0x05, 0x01, 0x25, 0x07, \
    0x46, 0x3B, 0x01, 0x75, 0x04, 0x95, 0x01, 0x65, 0x14, \
    0x09, 0x39, 0x81, 0x42, 0x65, 0x00, 0x95, 0x01, 0x81, 0x01, \
    0x26, 0xFF, 0x00, 0x46, 0xFF, 0x00, 0x09, 0x30, 0x09, 0x31, \
    0x09, 0x32, 0x09, 0x35, 0x75, 0x08, 0x95, 0x04, 0x81, 0x02, \
    0x06, 0x00, 0xFF, 0x09, 0x20, 0x95, 0x01, 0x81, 0x02, \
    0x0A, 0x21, 0x26, 0x95, 0x08, 0x91, 0x02, 0xC0

  static constexpr uint8_t reportDescriptor[] = {
    SWITCH_GAMEPAD_REPORT
  };

  #undef SWITCH_GAMEPAD_REPORT
};

constexpr uint8_t SwitchHID_::reportDescriptor[];

SwitchHID_ SwitchHID;
WebServer server(HTTP_PORT);
WiFiServer wsServer(WS_PORT);
WiFiClient wsClients[CONTROLLER_COUNT];
WiFiUDP udp;
Preferences prefs;

SwitchReport currentReports[CONTROLLER_COUNT];
uint32_t lastInputMs[CONTROLLER_COUNT] = {0};
uint32_t lastReportUs = 0;
uint8_t nextReportPlayer = 0;
uint32_t packetsSeen = 0;

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name="theme-color" content="#0b0e12">
<title>SwitchPadBridge</title>
<style>
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;width:100%;height:100%;overflow:hidden;background:#0b0e12;color:#f5f7fa;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;touch-action:none;user-select:none;-webkit-user-select:none}
button{font:inherit;color:inherit;touch-action:none;cursor:pointer}
.gamepad{position:fixed;inset:0;width:100vw;height:100vh;height:100dvh;overflow:hidden;background:#0b0e12}
.side{position:absolute;top:0;bottom:0;width:50%;min-width:300px}
.left{left:0}.right{right:0}
.utility{position:absolute;z-index:5;top:max(14px,env(safe-area-inset-top));left:50%;display:flex;gap:clamp(8px,1.4vw,18px);transform:translateX(-50%)}
.utility button,.shoulders button{border:1px solid #4b5561;background:#242a31;box-shadow:0 5px 12px #0008,inset 0 1px #ffffff12;font-weight:750}
.utility button{position:relative;width:clamp(44px,5.2vw,62px);height:clamp(38px,5vw,54px);border-radius:7px;font-size:clamp(14px,1.5vw,18px)}
.shoulders{position:absolute;z-index:3;top:max(14px,env(safe-area-inset-top));display:flex;flex-direction:column;gap:6px}
.left .shoulders{left:max(14px,env(safe-area-inset-left))}.right .shoulders{right:max(14px,env(safe-area-inset-right))}
.shoulders button{position:relative;width:clamp(64px,8vw,112px);height:clamp(34px,4.2vw,48px);border-radius:7px;font-size:clamp(14px,1.7vw,20px)}
.control{position:absolute}
.stick{position:fixed;z-index:4;width:clamp(112px,15vw,174px);aspect-ratio:1;border-radius:50%;background:#151a20;border:2px solid #49515b;box-shadow:0 8px 20px #0009,inset 0 0 0 10px #222830;opacity:0;pointer-events:none;transform:translate(-50%,-50%);transition:opacity 80ms ease}
.stick.active{opacity:1}
.stick:after{content:"";position:absolute;inset:19%;border:1px solid #555e68;border-radius:50%}
.nub{position:absolute;z-index:2;left:50%;top:50%;width:45%;aspect-ratio:1;border-radius:50%;transform:translate(-50%,-50%);background:#343b44;border:2px solid #626b76;box-shadow:0 5px 9px #0008}
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
.layout-tools{position:fixed;z-index:12;right:max(10px,env(safe-area-inset-right));bottom:max(10px,env(safe-area-inset-bottom));display:flex;gap:7px}
.layout-tools button{position:relative;width:38px;height:38px;display:grid;place-items:center;border:1px solid #4b5561;border-radius:7px;background:#20262d;box-shadow:0 4px 10px #0008}
.layout-tools svg{width:19px;height:19px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.player-select{display:none}
.edit-actions{display:none;gap:7px}.editing .edit-actions{display:flex}.editing #editLayout{display:none}
.editing [data-move]{outline:1px dashed #24d6cf;outline-offset:5px;cursor:move}.editing [data-move].selected{outline:2px solid #f5f7fa;outline-offset:6px}
.resize-handle{display:none;position:fixed;z-index:30;width:30px;height:30px;border:2px solid #0b0e12;border-radius:50%;background:#24d6cf;box-shadow:0 3px 9px #000b;touch-action:none;cursor:nwse-resize}.editing .resize-handle.active{display:block}
button.down,button:active{background:#3b4652!important;box-shadow:inset 0 2px 7px #000b!important;transform:translateY(1px)}
.face button.down,.face button:active{transform:translateY(-50%) scale(.96)}#bX.down,#bX:active,#bB.down,#bB:active{transform:translateX(-50%) scale(.96)}
.editing #bX:active,.editing #bB:active{transform:translateX(-50%)}.editing #bY:active,.editing #bA:active{transform:translateY(-50%)}
@media(orientation:portrait){
 .side{top:76px;width:50%;min-width:0}.utility{top:max(12px,env(safe-area-inset-top));gap:7px}.utility button{width:42px;height:40px;font-size:13px}.statusbar.connection{display:none}
 .shoulders{top:10px;gap:5px}.shoulders button{width:clamp(58px,17vw,92px);height:34px;font-size:13px}.left .shoulders{left:8px}.right .shoulders{right:8px}
 .stick{width:clamp(104px,34vw,164px)}
 .dpad{left:50%;bottom:8%;width:clamp(122px,38vw,174px);transform:translateX(-50%)}.face{right:50%;top:18%;width:clamp(142px,40vw,188px);transform:translateX(50%)}
 .statusbar.physical{max-width:76%;font-size:11px}
}
@media(orientation:landscape) and (max-height:430px){.statusbar.connection{display:none}.stick{width:clamp(104px,24vh,132px)}.face{width:clamp(146px,40vh,176px)}.dpad{width:clamp(122px,36vh,158px)}.face{top:25%}.dpad{bottom:4%}.shoulders button{height:38px}.utility button{height:38px}.statusbar.physical{bottom:4px}}
</style>
</head>
<body>
<main class="gamepad">
  <div class="utility" data-move="utility">
    <button data-bit="256" aria-label="Minus">-</button>
    <button data-bit="4096" aria-label="Home">HOME</button>
    <button data-bit="8192" aria-label="Capture">CAP</button>
    <button data-bit="512" aria-label="Plus">+</button>
  </div>
  <section class="side left" aria-label="Left controls">
    <div class="shoulders" data-move="left-shoulders"><button data-bit="16">L</button><button data-bit="64">ZL</button></div>
    <div id="stickL" class="control stick" data-x="lx" data-y="ly" data-click="1024"><div class="nub"></div></div>
    <div class="control dpad" data-move="dpad">
      <button class="up" data-dir="up" aria-label="Up"></button><button class="south" data-dir="down" aria-label="Down"></button>
      <button class="west" data-dir="left" aria-label="Left"></button><button class="east" data-dir="right" aria-label="Right"></button>
    </div>
  </section>
  <section class="side right" aria-label="Right controls">
    <div class="shoulders" data-move="right-shoulders"><button data-bit="32">R</button><button data-bit="128">ZR</button></div>
    <div class="control face" data-move="face">
      <button id="bX" data-bit="8" data-move="button-x">X</button><button id="bB" data-bit="2" data-move="button-b">B</button>
      <button id="bY" data-bit="1" data-move="button-y">Y</button><button id="bA" data-bit="4" data-move="button-a">A</button>
    </div>
    <div id="stickR" class="control stick" data-x="rx" data-y="ry" data-click="2048"><div class="nub"></div></div>
  </section>
  <div class="statusbar connection"><span id="dot" class="dot"></span><span id="status">connecting</span></div>
  <div id="gp" class="statusbar physical">touch controls</div>
  <button id="playerSelect" class="player-select" aria-label="Select player" title="Select player">P1</button>
  <div id="resizeHandle" class="resize-handle" aria-label="Resize selected control"></div>
  <div class="layout-tools">
    <button id="editLayout" aria-label="Edit layout" title="Edit layout"><svg viewBox="0 0 24 24"><path d="M21.2 6.8a2.1 2.1 0 0 0-4-4L3.8 16.2a2 2 0 0 0-.5.8L2 21.4a.5.5 0 0 0 .6.6L7 20.7a2 2 0 0 0 .8-.5z"/><path d="m15 5 4 4"/></svg></button>
    <div class="edit-actions">
      <button id="resetLayout" aria-label="Reset layout" title="Reset to default"><svg viewBox="0 0 24 24"><path d="M3 12a9 9 0 1 0 3-6.7L3 8"/><path d="M3 3v5h5"/></svg></button>
      <button id="cancelLayout" aria-label="Cancel editing" title="Cancel"><svg viewBox="0 0 24 24"><path d="M18 6 6 18M6 6l12 12"/></svg></button>
      <button id="saveLayout" aria-label="Save layout" title="Save layout"><svg viewBox="0 0 24 24"><path d="m20 6-11 11-5-5"/></svg></button>
    </div>
  </div>
</main>
<script>
const neutral=()=>({buttons:0,hat:8,lx:128,ly:128,rx:128,ry:128});
const touch=neutral(), physical=neutral();
const activeSticks={lx:false,rx:false};
const dirs=new Set();
let ws,connected=false,lastSent="",physicalConnected=false,wsFailures=0,useHttp=false,httpBusy=false,editing=false;
let player=0;
const dot=document.getElementById("dot"),statusEl=document.getElementById("status"),gpEl=document.getElementById("gp");
const playerSelect=document.getElementById("playerSelect");
function showPlayer(){playerSelect.textContent=`P${player+1}`;playerSelect.title=`Controller ${player+1}`}
showPlayer();
function connect(){
  if(useHttp)return;
  ws=new WebSocket(`ws://${location.hostname}:81/ws`);
  ws.onopen=()=>{connected=true;dot.classList.add("ok");statusEl.textContent="connected";lastSent=""};
  ws.onclose=()=>{connected=false;wsFailures++;if(wsFailures>=3){useHttp=true;dot.classList.add("ok");statusEl.textContent="connected";send(true);return}dot.classList.remove("ok");statusEl.textContent="reconnecting";setTimeout(connect,700)};
}
connect();
function merged(){
  return {buttons:touch.buttons|physical.buttons,hat:touch.hat!==8?touch.hat:physical.hat,
    lx:activeSticks.lx?touch.lx:physicalConnected?physical.lx:touch.lx,
    ly:activeSticks.lx?touch.ly:physicalConnected?physical.ly:touch.ly,
    rx:activeSticks.rx?touch.rx:physicalConnected?physical.rx:touch.rx,
    ry:activeSticks.rx?touch.ry:physicalConnected?physical.ry:touch.ry};
}
function payload(){const s=merged();return `player=${player}&buttons=${s.buttons}&hat=${s.hat}&lx=${s.lx}&ly=${s.ly}&rx=${s.rx}&ry=${s.ry}`}
function send(force=false){
  const p=payload();if(!force&&p===lastSent)return;
  if(connected&&ws?.readyState===WebSocket.OPEN){ws.send(p);lastSent=p;return}
  if(useHttp&&!httpBusy){httpBusy=true;fetch("/api/input",{method:"POST",headers:{"Content-Type":"text/plain"},body:p}).then(r=>{if(!r.ok)throw Error() ;dot.classList.add("ok");statusEl.textContent="connected";lastSent=p}).catch(()=>{dot.classList.remove("ok");statusEl.textContent="reconnecting"}).finally(()=>httpBusy=false)}
}
function pressButton(el,on){
  const bit=Number(el.dataset.bit);touch.buttons=on?touch.buttons|bit:touch.buttons&~bit;el.classList.toggle("down",on);send();
  if(on&&navigator.vibrate)navigator.vibrate(8);
}
for(const b of document.querySelectorAll("button[data-bit]")){
  b.addEventListener("pointerdown",e=>{if(editing)return;e.preventDefault();b.setPointerCapture(e.pointerId);pressButton(b,true)});
  const off=e=>{if(editing)return;e.preventDefault();pressButton(b,false)};
  b.addEventListener("pointerup",off);b.addEventListener("pointercancel",off);b.addEventListener("lostpointercapture",()=>pressButton(b,false));
}
function updateHat(){
  const u=dirs.has("up"),d=dirs.has("down"),l=dirs.has("left"),r=dirs.has("right");
  touch.hat=u&&r?1:r&&d?3:d&&l?5:l&&u?7:u?0:r?2:d?4:l?6:8;send();
}
for(const b of document.querySelectorAll("button[data-dir]")){
  b.addEventListener("pointerdown",e=>{if(editing)return;e.preventDefault();b.setPointerCapture(e.pointerId);dirs.add(b.dataset.dir);b.classList.add("down");updateHat()});
  const off=e=>{if(editing)return;e.preventDefault();dirs.delete(b.dataset.dir);b.classList.remove("down");updateHat()};
  b.addEventListener("pointerup",off);b.addEventListener("pointercancel",off);b.addEventListener("lostpointercapture",off);
}
for(const zone of document.querySelectorAll(".side")){
  const stick=zone.querySelector(".stick"),nub=stick.querySelector(".nub"),x=stick.dataset.x,y=stick.dataset.y;let pointer=null,baseX=0,baseY=0;
  const move=e=>{const max=stick.getBoundingClientRect().width*.31;let dx=e.clientX-baseX,dy=e.clientY-baseY,d=Math.hypot(dx,dy);if(d>max){dx=dx/d*max;dy=dy/d*max}nub.style.transform=`translate(calc(-50% + ${dx}px),calc(-50% + ${dy}px))`;touch[x]=Math.round(128+dx/max*127);touch[y]=Math.round(128+dy/max*127);send()};
  zone.addEventListener("pointerdown",e=>{if(editing||pointer!==null||e.target.closest("button,.dpad,.face,.shoulders"))return;e.preventDefault();pointer=e.pointerId;baseX=e.clientX;baseY=e.clientY;stick.style.left=`${baseX}px`;stick.style.top=`${baseY}px`;stick.classList.add("active");activeSticks[x]=true;zone.setPointerCapture(pointer);move(e)});
  zone.addEventListener("pointermove",e=>{if(e.pointerId===pointer)move(e)});
  const end=e=>{if(e.pointerId!==pointer)return;pointer=null;activeSticks[x]=false;stick.classList.remove("active");nub.style.transform="translate(-50%,-50%)";touch[x]=128;touch[y]=128;send()};
  zone.addEventListener("pointerup",end);zone.addEventListener("pointercancel",end);zone.addEventListener("lostpointercapture",end);
}
const movable=[...document.querySelectorAll("[data-move]")];
const layoutKey=()=>`switchpad-layout-v1-${matchMedia("(orientation:portrait)").matches?"portrait":"landscape"}`;
function setOffset(el,x,y){el.dataset.moveX=String(Math.round(x));el.dataset.moveY=String(Math.round(y));el.style.translate=`${Math.round(x)}px ${Math.round(y)}px`}
function setSize(el,w,h){if(w){el.dataset.sizeW=String(Math.round(w));el.style.width=`${Math.round(w)}px`}else{delete el.dataset.sizeW;el.style.removeProperty("width")}if(h){el.dataset.sizeH=String(Math.round(h));el.style.height=`${Math.round(h)}px`}else{delete el.dataset.sizeH;el.style.removeProperty("height")}}
function readLayout(){return Object.fromEntries(movable.map(el=>[el.dataset.move,{x:Number(el.dataset.moveX)||0,y:Number(el.dataset.moveY)||0,w:Number(el.dataset.sizeW)||0,h:Number(el.dataset.sizeH)||0}]))}
function applyLayout(layout={}){for(const el of movable){const p=layout[el.dataset.move]||{x:0,y:0,w:0,h:0};setOffset(el,p.x,p.y);setSize(el,p.w,p.h)}positionResizeHandle()}
function loadLayout(){try{applyLayout(JSON.parse(localStorage.getItem(layoutKey())||"{}"))}catch(e){applyLayout()}}
const resizeHandle=document.getElementById("resizeHandle");
let editSnapshot={},selected=null;
function selectControl(el){if(selected)selected.classList.remove("selected");selected=el;if(selected)selected.classList.add("selected");positionResizeHandle()}
function positionResizeHandle(){if(!editing||!selected){resizeHandle.classList.remove("active");return}const r=selected.getBoundingClientRect();resizeHandle.style.left=`${r.right-15}px`;resizeHandle.style.top=`${r.bottom-15}px`;resizeHandle.classList.add("active")}
function finishEdit(){selectControl(null);editing=false;document.body.classList.remove("editing")}
document.getElementById("editLayout").addEventListener("click",()=>{editSnapshot=readLayout();Object.assign(touch,neutral());dirs.clear();send(true);editing=true;document.body.classList.add("editing");selectControl(document.querySelector('[data-move="face"]'))});
document.getElementById("resetLayout").addEventListener("click",()=>applyLayout());
document.getElementById("cancelLayout").addEventListener("click",()=>{applyLayout(editSnapshot);finishEdit()});
document.getElementById("saveLayout").addEventListener("click",()=>{try{localStorage.setItem(layoutKey(),JSON.stringify(readLayout()))}catch(e){}finishEdit()});
for(const el of movable){
  let pointer=null,startX=0,startY=0,originX=0,originY=0;
  el.addEventListener("pointerdown",e=>{if(!editing||pointer!==null)return;e.preventDefault();e.stopPropagation();selectControl(el);pointer=e.pointerId;startX=e.clientX;startY=e.clientY;originX=Number(el.dataset.moveX)||0;originY=Number(el.dataset.moveY)||0;el.setPointerCapture(pointer)});
  el.addEventListener("pointermove",e=>{if(e.pointerId!==pointer)return;let x=originX+e.clientX-startX,y=originY+e.clientY-startY;setOffset(el,x,y);const r=el.getBoundingClientRect(),pad=6;if(r.left<pad)x+=pad-r.left;if(r.right>innerWidth-pad)x-=r.right-(innerWidth-pad);if(r.top<pad)y+=pad-r.top;if(r.bottom>innerHeight-pad)y-=r.bottom-(innerHeight-pad);setOffset(el,x,y);positionResizeHandle()});
  const end=e=>{if(e.pointerId===pointer)pointer=null};el.addEventListener("pointerup",end);el.addEventListener("pointercancel",end);el.addEventListener("lostpointercapture",end);
}
{
  let pointer=null,startX=0,startY=0,startW=0,startH=0;
  resizeHandle.addEventListener("pointerdown",e=>{if(!editing||!selected)return;e.preventDefault();e.stopPropagation();pointer=e.pointerId;startX=e.clientX;startY=e.clientY;const r=selected.getBoundingClientRect();startW=r.width;startH=r.height;resizeHandle.setPointerCapture(pointer)});
  resizeHandle.addEventListener("pointermove",e=>{if(e.pointerId!==pointer||!selected)return;setSize(selected,Math.max(30,startW+e.clientX-startX),Math.max(30,startH+e.clientY-startY));positionResizeHandle()});
  const end=e=>{if(e.pointerId===pointer)pointer=null};resizeHandle.addEventListener("pointerup",end);resizeHandle.addEventListener("pointercancel",end);resizeHandle.addEventListener("lostpointercapture",end);
}
loadLayout();
window.addEventListener("orientationchange",()=>setTimeout(()=>{if(!editing)loadLayout();else positionResizeHandle()},150));
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
  uint8_t player = (uint8_t)parseNumber(getParam(body, "player"), 0);
  if (player >= CONTROLLER_COUNT) return;

  noInterrupts();
  SwitchReport next = currentReports[player];
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
  currentReports[player] = next;
  interrupts();
  lastInputMs[player] = millis();
  packetsSeen++;
}

String stateJson() {
  String out = "{";
  out += "\"controllers\":" + String(CONTROLLER_COUNT) + ",";
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

bool waitForWsBytes(WiFiClient &client, size_t count) {
  uint32_t start = millis();
  while (client.connected() && client.available() < (int)count) {
    if (millis() - start >= WS_READ_TIMEOUT_MS) return false;
    delay(1);
  }
  return client.connected() && client.available() >= (int)count;
}

void sendWsControlFrame(WiFiClient &client, uint8_t opcode, const uint8_t *payload, size_t len) {
  if (!client.connected() || len > 125) return;
  uint8_t header[2] = {(uint8_t)(0x80 | opcode), (uint8_t)len};
  client.write(header, sizeof(header));
  if (len) client.write(payload, len);
}

void pollWsClient(WiFiClient &client) {
  if (!client || !client.connected() || client.available() < 2) return;

  uint8_t b0 = client.read();
  uint8_t b1 = client.read();
  uint8_t opcode = b0 & 0x0F;
  bool masked = b1 & 0x80;
  uint64_t len = b1 & 0x7F;

  if (len == 126) {
    if (!waitForWsBytes(client, 2)) {
      client.stop();
      return;
    }
    len = ((uint16_t)client.read() << 8) | client.read();
  } else if (len == 127) {
    client.stop();
    return;
  }

  if (!masked || len > 512) {
    client.stop();
    return;
  }

  uint8_t mask[4] = {0, 0, 0, 0};
  if (!waitForWsBytes(client, 4)) {
    client.stop();
    return;
  }
  for (int i = 0; i < 4; i++) mask[i] = client.read();

  String msg = "";
  msg.reserve((size_t)len);
  for (uint64_t i = 0; i < len; i++) {
    if (!waitForWsBytes(client, 1)) {
      client.stop();
      return;
    }
    char c = client.read();
    c ^= mask[i & 3];
    msg += c;
  }

  if (opcode == 0x8) {
    client.stop();
  } else if (opcode == 0x9) {
    sendWsControlFrame(client, 0xA, (const uint8_t *)msg.c_str(), msg.length());
  } else if (opcode == 0x1 && msg.length()) {
    applyInput(msg);
  }
}

void pollWebSocket() {
  WiFiClient next = wsServer.available();
  if (next) {
    int slot = -1;
    for (uint8_t i = 0; i < CONTROLLER_COUNT; i++) {
      if (!wsClients[i] || !wsClients[i].connected()) {
        slot = i;
        break;
      }
    }
    if (slot >= 0) {
      wsClients[slot] = next;
      handleWsHandshake(wsClients[slot]);
    } else {
      next.stop();
    }
  }
  for (uint8_t i = 0; i < CONTROLLER_COUNT; i++) pollWsClient(wsClients[i]);
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
  if (!WiFi.config(STATIC_IP, GATEWAY_IP, SUBNET_MASK, GATEWAY_IP)) {
    Serial.println("Static IP configuration failed.");
  }
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
  MDNS.setInstanceName("SwitchPadBridge");
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
  for (uint8_t i = 0; i < CONTROLLER_COUNT; i++) {
    currentReports[i] = {0, 8, 128, 128, 128, 128, 0};
    lastInputMs[i] = millis();
  }
  if (!connectWifi()) {
    startSetupAccessPoint();
  }
  setupHttp();
  wsServer.begin();
  udp.begin(UDP_PORT);
  setupUsb();
  Serial.println("Open the controller page on your phone:");
  Serial.print("http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
  pollWebSocket();
  pollUdp();

  for (uint8_t i = 0; i < CONTROLLER_COUNT; i++) {
    if (millis() - lastInputMs[i] > INPUT_TIMEOUT_MS) {
      noInterrupts();
      currentReports[i] = {0, 8, 128, 128, 128, 128, 0};
      interrupts();
    }
  }

  uint32_t now = micros();
  if (now - lastReportUs >= REPORT_INTERVAL_US) {
    lastReportUs = now;
    noInterrupts();
    SwitchReport report = currentReports[nextReportPlayer];
    interrupts();
    SwitchHID.send(nextReportPlayer, report);
    nextReportPlayer = (nextReportPlayer + 1) % CONTROLLER_COUNT;
  }
}
