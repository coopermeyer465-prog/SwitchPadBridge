#pragma once

static const char INDEX_HTML[] = R"HTML(
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
.gamepad{position:fixed;inset:0;width:100vw;height:100vh;height:100dvh;overflow:hidden;background-color:#0b0e12;background-size:cover;background-position:center;background-repeat:no-repeat}
.side{position:absolute;top:0;bottom:0;width:50%;min-width:300px}
.left{left:0}.right{right:0}
.utility{position:absolute;z-index:5;top:max(14px,env(safe-area-inset-top));left:50%;display:flex;gap:clamp(8px,1.4vw,18px);transform:translateX(-50%)}
.utility button,.shoulders button{border:1px solid #4b5561;background:#242a31;box-shadow:0 5px 12px #0008,inset 0 1px #ffffff12;font-weight:750}
.utility button{position:relative;width:clamp(44px,5.2vw,62px);height:clamp(38px,5vw,54px);border-radius:7px;font-size:clamp(14px,1.5vw,18px)}
.utility button svg{width:58%;height:58%;fill:none;stroke:currentColor;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round}
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
.dpad button{position:absolute;width:34%;height:42%;border:0;background:#2a3037;font-size:0}
.dpad button:after{content:"";position:absolute;left:50%;top:50%;width:0;height:0;transform:translate(-50%,-50%);border:8px solid transparent}
.dpad .up{left:33%;top:0;border-radius:7px 7px 2px 2px}.dpad .south{left:33%;bottom:0;border-radius:2px 2px 7px 7px}.dpad .west{left:0;top:29%;width:42%;height:34%;border-radius:7px 2px 2px 7px}.dpad .east{right:0;top:29%;width:42%;height:34%;border-radius:2px 7px 7px 2px}
.dpad .up:after{border-bottom-color:#eef2f6;margin-top:-5px}.dpad .south:after{border-top-color:#eef2f6;margin-top:5px}.dpad .west:after{border-right-color:#eef2f6;margin-left:-5px}.dpad .east:after{border-left-color:#eef2f6;margin-left:5px}
.dpad:after{content:"";position:absolute;pointer-events:none;left:38%;top:38%;width:24%;height:24%;border-radius:50%;background:#2a3037}
.statusbar{position:absolute;z-index:6;display:flex;align-items:center;gap:10px;color:#9ca8b5;font-size:12px}
.statusbar.connection{top:max(74px,calc(env(safe-area-inset-top) + 62px));left:50%;transform:translateX(-50%)}
.statusbar.physical{display:none}
.dot{width:9px;height:9px;flex:none;border-radius:50%;background:#e05555;box-shadow:0 0 0 3px #e0555525}.dot.ok{background:#24d6cf;box-shadow:0 0 0 3px #24d6cf28}
.layout-tools{position:fixed;z-index:12;right:max(10px,env(safe-area-inset-right));bottom:max(10px,env(safe-area-inset-bottom));display:flex;gap:7px}
.layout-tools button{position:relative;width:38px;height:38px;display:grid;place-items:center;border:1px solid #4b5561;border-radius:7px;background:#20262d;box-shadow:0 4px 10px #0008}
.layout-tools button:disabled{opacity:.35;cursor:default}
.layout-tools svg{width:19px;height:19px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.layout-tools #swapSticks.active{background:#176b69;border-color:#24d6cf}
.layout-tools #chooseBackground.active{background:#176b69;border-color:#24d6cf}.no-background #removeBackground{display:none}
.desktop-map-only{display:none!important}
.desktop-mode-button{display:none;position:fixed;z-index:12;left:10px;bottom:10px;width:42px;height:42px;place-items:center;border:1px solid #4b5561;border-radius:7px;background:#20262d;box-shadow:0 4px 10px #0008}.desktop-mode-button svg{width:21px;height:21px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}.desktop-mode .desktop-mode-button{background:#176b69;border-color:#24d6cf}.editing .desktop-mode-button{display:none}
.player-select{display:none}
.edit-actions{display:none;gap:7px}.editing .edit-actions{display:flex}.editing #editLayout{display:none}
.editing [data-move]{outline:1px dashed #24d6cf;outline-offset:5px;cursor:move}.editing [data-move].selected{outline:2px solid #f5f7fa;outline-offset:6px}
.resize-handle{display:none;position:fixed;z-index:30;width:30px;height:30px;border:2px solid #0b0e12;border-radius:50%;background:#24d6cf;box-shadow:0 3px 9px #000b;touch-action:none;cursor:nwse-resize}.editing .resize-handle.active{display:block}
button.down,button.desktop-lit,button:active{background:#3b4652!important;box-shadow:inset 0 2px 7px #000b!important;transform:translateY(1px)}
.face button.down,.face button.desktop-lit,.face button:active{transform:translateY(-50%) scale(.96)}#bX.down,#bX.desktop-lit,#bX:active,#bB.down,#bB.desktop-lit,#bB:active{transform:translateX(-50%) scale(.96)}
.editing #bX:active,.editing #bB:active{transform:translateX(-50%)}.editing #bY:active,.editing #bA:active{transform:translateY(-50%)}
.stick-indicator{display:none;position:absolute;z-index:8;width:clamp(92px,13vw,146px);aspect-ratio:1;place-items:center;border:2px dashed #24d6cf;border-radius:50%;background:#0b0e1266;color:#24d6cf;font-weight:850;font-size:24px;pointer-events:none}.editing .stick-indicator{display:grid}#leftStickIndicator{left:max(5vw,env(safe-area-inset-left));top:27%}#rightStickIndicator{right:max(6vw,env(safe-area-inset-right));bottom:18%}
.desktop-mode .stick{opacity:1}.desktop-mode #stickL{left:12%!important;top:42%!important}.desktop-mode #stickR{left:88%!important;top:68%!important}.desktop-mode .stick.desktop-click{border-color:#24d6cf;box-shadow:0 0 0 4px #24d6cf35,0 8px 20px #0009,inset 0 0 0 10px #222830}
.keymap-panel{display:none;position:fixed;z-index:40;inset:7vh max(18px,calc((100vw - 680px)/2));overflow:auto;border:1px solid #4b5561;border-radius:8px;background:#11161cfa;box-shadow:0 18px 50px #000d;padding:16px;touch-action:auto}.keymap-panel.open{display:block}.keymap-header{display:flex;align-items:center;justify-content:space-between;margin-bottom:14px}.keymap-header strong{font-size:17px}.keymap-header button,.keymap-footer button{width:38px;height:38px;border:1px solid #4b5561;border-radius:7px;background:#20262d}.keymap-footer svg{width:19px;height:19px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}.keymap-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px 14px}.keymap-row{display:grid;grid-template-columns:1fr minmax(110px,150px);align-items:center;gap:10px;min-height:38px;color:#c8d0d9;font-size:13px}.keymap-row button,.keymap-row select{height:34px;min-width:0;border:1px solid #4b5561;border-radius:6px;background:#20262d;color:#f5f7fa;padding:0 8px}.keymap-row input{width:100%}.keymap-footer{display:flex;justify-content:flex-end;margin-top:14px}.capturing{border-color:#24d6cf!important;color:#24d6cf!important}
@media(hover:hover) and (pointer:fine){.desktop-mode-button{display:grid}.editing .desktop-map-only{display:grid!important}.editing [data-key-hint]::before{content:attr(data-key-hint);position:absolute;z-index:20;left:50%;top:-17px;min-width:max-content;padding:2px 5px;border:1px solid #55b8ff;border-radius:4px;background:#09263d;color:#7dcbff;font-size:10px;font-weight:800;line-height:1;transform:translateX(-50%);pointer-events:none}.editing .stick-indicator[data-key-hint]::before{top:auto;bottom:8px;font-size:11px}}
@media(orientation:portrait){
 .side{top:76px;width:50%;min-width:0}.utility{top:max(12px,env(safe-area-inset-top));gap:7px}.utility button{width:42px;height:40px;font-size:13px}.statusbar.connection{display:none}
 .shoulders{top:10px;gap:5px}.shoulders button{width:clamp(58px,17vw,92px);height:34px;font-size:13px}.left .shoulders{left:8px}.right .shoulders{right:8px}
 .stick{width:clamp(104px,34vw,164px)}
 .desktop-mode #stickL{left:25%!important;top:29%!important}.desktop-mode #stickR{left:75%!important;top:65%!important}
 #leftStickIndicator{left:50%;top:13%;transform:translateX(-50%)}#rightStickIndicator{right:50%;bottom:18%;transform:translateX(50%)}
 .dpad{left:50%;bottom:8%;width:clamp(122px,38vw,174px);transform:translateX(-50%)}.face{right:50%;top:18%;width:clamp(142px,40vw,188px);transform:translateX(50%)}
 .statusbar.physical{max-width:76%;font-size:11px}
}
@media(orientation:landscape) and (max-width:900px) and (max-height:430px){
 .statusbar.connection{display:none}.stick{width:clamp(96px,25vh,116px)}
 .utility{top:max(8px,env(safe-area-inset-top));gap:6px}.utility button{width:38px;height:34px;font-size:12px}
 .shoulders{top:max(8px,env(safe-area-inset-top));gap:4px}.shoulders button{width:58px;height:31px;font-size:13px}
 .face{top:31%;width:clamp(126px,34vh,142px)}.face button{width:clamp(46px,12vh,52px);font-size:18px}
 .dpad{bottom:6%;width:clamp(108px,30vh,126px)}.dpad button:after{border-width:6px}
 .statusbar.physical{bottom:3px;max-width:28%}.layout-tools{bottom:max(6px,env(safe-area-inset-bottom))}
 #leftStickIndicator{top:27%}#rightStickIndicator{bottom:15%}
}
</style>
</head>
<body class="no-background">
<main class="gamepad">
  <div class="utility" data-move="utility">
    <button data-bit="256" data-move="button-minus" aria-label="Minus">-</button>
    <button data-bit="4096" data-move="button-home" aria-label="Home"><svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="9"/><path d="m7.5 11.5 4.5-4 4.5 4V17h-3v-3h-3v3h-3z"/></svg></button>
    <button data-bit="8192" data-move="button-capture" aria-label="Capture"><svg viewBox="0 0 24 24" aria-hidden="true"><rect x="3.5" y="3.5" width="17" height="17" rx="1.5"/><circle cx="12" cy="12" r="5" fill="currentColor" stroke="none"/></svg></button>
    <button data-bit="512" data-move="button-plus" aria-label="Plus">+</button>
  </div>
  <section class="side left" aria-label="Left controls">
    <div id="leftStickIndicator" class="stick-indicator">L</div>
    <div class="shoulders" data-move="left-shoulders"><button data-bit="16" data-move="button-l">L</button><button data-bit="64" data-move="button-zl">ZL</button></div>
    <div id="stickL" class="control stick" data-x="lx" data-y="ly" data-click="1024"><div class="nub"></div></div>
    <div class="control dpad" data-move="dpad">
      <button class="up" data-dir="up" aria-label="Up"></button><button class="south" data-dir="down" aria-label="Down"></button>
      <button class="west" data-dir="left" aria-label="Left"></button><button class="east" data-dir="right" aria-label="Right"></button>
    </div>
  </section>
  <section class="side right" aria-label="Right controls">
    <div id="rightStickIndicator" class="stick-indicator">R</div>
    <div class="shoulders" data-move="right-shoulders"><button data-bit="32" data-move="button-r">R</button><button data-bit="128" data-move="button-zr">ZR</button></div>
    <div class="control face" data-move="face">
      <button id="bX" data-bit="8" data-move="button-x">X</button><button id="bB" data-bit="2" data-move="button-b">B</button>
      <button id="bY" data-bit="1" data-move="button-y">Y</button><button id="bA" data-bit="4" data-move="button-a">A</button>
    </div>
    <div id="stickR" class="control stick" data-x="rx" data-y="ry" data-click="2048"><div class="nub"></div></div>
  </section>
  <div class="statusbar connection"><span id="dot" class="dot"></span><span id="status">connecting</span></div>
  <div id="gp" class="statusbar physical">touch controls</div>
  <button id="desktopMode" class="desktop-mode-button" aria-label="Enter desktop controller mode" title="Desktop controller mode"><svg viewBox="0 0 24 24"><path d="M8 3H3v5M16 3h5v5M8 21H3v-5M16 21h5v-5"/></svg></button>
  <button id="playerSelect" class="player-select" aria-label="Select player" title="Select player">P1</button>
  <div id="resizeHandle" class="resize-handle" aria-label="Resize selected control"></div>
  <input id="backgroundPhoto" type="file" accept="image/*" hidden>
  <section id="keymapPanel" class="keymap-panel" aria-label="Desktop control mappings">
    <div class="keymap-header"><strong>Desktop controls</strong><button id="closeKeymap" aria-label="Close mappings">X</button></div>
    <div id="keymapGrid" class="keymap-grid"></div>
    <div class="keymap-footer"><button id="resetKeymap" aria-label="Reset desktop mappings" title="Reset mappings"><svg viewBox="0 0 24 24"><path d="M3 12a9 9 0 1 0 3-6.7L3 8"/><path d="M3 3v5h5"/></svg></button></div>
  </section>
  <div class="layout-tools">
    <button id="editLayout" aria-label="Edit layout" title="Edit layout"><svg viewBox="0 0 24 24"><path d="M21.2 6.8a2.1 2.1 0 0 0-4-4L3.8 16.2a2 2 0 0 0-.5.8L2 21.4a.5.5 0 0 0 .6.6L7 20.7a2 2 0 0 0 .8-.5z"/><path d="m15 5 4 4"/></svg></button>
    <div class="edit-actions">
      <button id="editKeymap" class="desktop-map-only" aria-label="Edit keyboard and mouse mappings" title="Desktop mappings"><svg viewBox="0 0 24 24"><rect x="3" y="5" width="18" height="14" rx="2"/><path d="M7 9h.01M11 9h.01M15 9h.01M7 13h.01M11 13h.01M15 13h2M7 17h10"/></svg></button>
      <button id="chooseBackground" aria-label="Choose background photo" title="Background photo"><svg viewBox="0 0 24 24"><rect x="3" y="5" width="18" height="14" rx="2"/><circle cx="8.5" cy="10" r="1.5"/><path d="m21 15-5-5L5 19"/></svg></button>
      <button id="removeBackground" aria-label="Remove background photo" title="Remove background"><svg viewBox="0 0 24 24"><path d="M3 6h18M8 6V4h8v2M19 6l-1 15H6L5 6M10 11v6M14 11v6"/></svg></button>
      <button id="swapSticks" aria-label="Swap analog stick sides" title="Swap sticks"><svg viewBox="0 0 24 24"><path d="M7 7h11l-3-3"/><path d="m18 7-3 3"/><path d="M17 17H6l3 3"/><path d="m6 17 3-3"/></svg></button>
      <button id="undoLayout" aria-label="Undo layout change" title="Undo"><svg viewBox="0 0 24 24"><path d="M9 7 4 12l5 5"/><path d="M4 12h9a7 7 0 0 1 7 7"/></svg></button>
      <button id="resetLayout" aria-label="Reset layout" title="Reset to default"><svg viewBox="0 0 24 24"><path d="M3 12a9 9 0 1 0 3-6.7L3 8"/><path d="M3 3v5h5"/></svg></button>
      <button id="cancelLayout" aria-label="Cancel editing" title="Cancel"><svg viewBox="0 0 24 24"><path d="M18 6 6 18M6 6l12 12"/></svg></button>
      <button id="saveLayout" aria-label="Save layout" title="Save layout"><svg viewBox="0 0 24 24"><path d="m20 6-11 11-5-5"/></svg></button>
    </div>
  </div>
</main>
<script>
const neutral=()=>({buttons:0,hat:8,lx:128,ly:128,rx:128,ry:128});
const touch=neutral(),physical=neutral(),desktop=neutral();
const activeSticks={lx:false,rx:false},desktopAxes={lx:false,rx:false};
const dirs=new Set();
let ws,connected=false,lastSent="",physicalConnected=false,useHttp=true,httpBusy=false,pendingPayload="",editing=false,lastWireMs=0,sendTimer=0,claimed=false;
let player=0,sticksSwapped=false,desktopMode=false,mouseVX=0,mouseVY=0;
const desktopKeys=new Set(),desktopMouseButtons=new Set();
const dot=document.getElementById("dot"),statusEl=document.getElementById("status"),gpEl=document.getElementById("gp");
const playerSelect=document.getElementById("playerSelect");
const gamepadEl=document.querySelector(".gamepad"),photoInput=document.getElementById("backgroundPhoto"),chooseBackground=document.getElementById("chooseBackground"),removeBackground=document.getElementById("removeBackground");
const backgroundKey="switchpad-background-v1";
function applyBackground(data=""){gamepadEl.style.backgroundImage=data?`url("${data}")`:"";document.body.classList.toggle("no-background",!data);chooseBackground.classList.toggle("active",!!data)}
function loadBackground(){try{applyBackground(localStorage.getItem(backgroundKey)||"")}catch(e){applyBackground()}}
chooseBackground.addEventListener("click",()=>photoInput.click());
removeBackground.addEventListener("click",()=>{try{localStorage.removeItem(backgroundKey)}catch(e){}applyBackground()});
photoInput.addEventListener("change",()=>{const file=photoInput.files&&photoInput.files[0];photoInput.value="";if(!file)return;const img=new Image(),url=URL.createObjectURL(file);img.onload=()=>{const scale=Math.min(1,1280/img.width,1280/img.height),canvas=document.createElement("canvas");canvas.width=Math.max(1,Math.round(img.width*scale));canvas.height=Math.max(1,Math.round(img.height*scale));canvas.getContext("2d").drawImage(img,0,0,canvas.width,canvas.height);URL.revokeObjectURL(url);const data=canvas.toDataURL("image/jpeg",.76);try{localStorage.setItem(backgroundKey,data);applyBackground(data)}catch(e){statusEl.textContent="photo too large"}};img.onerror=()=>{URL.revokeObjectURL(url);statusEl.textContent="photo unsupported"};img.src=url});
loadBackground();
function showPlayer(){playerSelect.textContent=`P${player+1}`;playerSelect.title=`Controller ${player+1}`}
showPlayer();
const deviceKey="switchpad-device-v1";
function loadDeviceId(){
  try{const saved=localStorage.getItem(deviceKey);if(saved)return saved;const bytes=new Uint8Array(12);crypto.getRandomValues(bytes);const id=[...bytes].map(x=>x.toString(16).padStart(2,"0")).join("");localStorage.setItem(deviceKey,id);return id}catch(e){return `${Date.now().toString(36)}-${Math.random().toString(36).slice(2)}`}
}
const deviceId=loadDeviceId();
async function claimController(){
  try{
    const response=await fetch(`/api/claim?device=${encodeURIComponent(deviceId)}`,{cache:"no-store"});
    if(!response.ok)throw Error(response.status===503?"full":"offline");
    const data=await response.json();player=data.player;claimed=true;showPlayer();dot.classList.add("ok");statusEl.textContent=`connected · P${player+1}`;connect();send(true);
  }catch(e){claimed=false;dot.classList.remove("ok");statusEl.textContent=e.message==="full"?"4 controllers in use":"reconnecting";setTimeout(claimController,1000)}
}
function connect(){
  if(!claimed)return;
  ws=new WebSocket(`ws://${location.host}/ws?device=${encodeURIComponent(deviceId)}`);
  ws.onopen=()=>{connected=true;useHttp=false;dot.classList.add("ok");statusEl.textContent=`connected · P${player+1}`;lastSent="";send(true)};
  ws.onclose=()=>{connected=false;useHttp=true;if(claimed){dot.classList.add("ok");statusEl.textContent=`connected · P${player+1}`;send(true);setTimeout(connect,1000)}};
  ws.onerror=()=>ws.close();
}
claimController();
function merged(){
  return {buttons:touch.buttons|physical.buttons|desktop.buttons,hat:touch.hat!==8?touch.hat:desktop.hat!==8?desktop.hat:physical.hat,
    lx:activeSticks.lx?touch.lx:desktopAxes.lx?desktop.lx:physicalConnected?physical.lx:touch.lx,
    ly:activeSticks.lx?touch.ly:desktopAxes.lx?desktop.ly:physicalConnected?physical.ly:touch.ly,
    rx:activeSticks.rx?touch.rx:desktopAxes.rx?desktop.rx:physicalConnected?physical.rx:touch.rx,
    ry:activeSticks.rx?touch.ry:desktopAxes.rx?desktop.ry:physicalConnected?physical.ry:touch.ry};
}
function payload(){const s=merged();return `device=${deviceId}&buttons=${s.buttons}&hat=${s.hat}&lx=${s.lx}&ly=${s.ly}&rx=${s.rx}&ry=${s.ry}`}
function postInput(p){
  httpBusy=true;lastSent=p;
  fetch("/api/input",{method:"POST",headers:{"Content-Type":"text/plain"},body:p}).then(r=>{if(!r.ok)throw Error(r.status===503?"full":"offline");dot.classList.add("ok");statusEl.textContent=`connected · P${player+1}`}).catch(e=>{dot.classList.remove("ok");statusEl.textContent=e.message==="full"?"4 controllers in use":"reconnecting";claimed=false;setTimeout(claimController,700)}).finally(()=>{httpBusy=false;if(pendingPayload){const next=pendingPayload;pendingPayload="";if(claimed&&next!==lastSent)postInput(next)}})
}
function send(force=false){
  if(!claimed)return;const p=payload();if(!force&&p===lastSent)return;
  const now=performance.now();
  if(!force&&now-lastWireMs<8){if(!sendTimer)sendTimer=setTimeout(()=>{sendTimer=0;send()},Math.max(1,8-(performance.now()-lastWireMs)));return}
  lastWireMs=now;
  if(connected&&ws?.readyState===WebSocket.OPEN){ws.send(p);lastSent=p;return}
  if(useHttp){if(httpBusy){pendingPayload=p;return}postInput(p)}
}
const desktopButton=document.getElementById("desktopMode");
const mappingKey="switchpad-desktop-map-v2";
const legacyMappingKey="switchpad-desktop-map-v1";
const mappingActions=[
  ["leftUp","Left stick up","KeyW"],["leftDown","Left stick down","KeyS"],["leftLeft","Left stick left","KeyA"],["leftRight","Left stick right","KeyD"],
  ["dpadUp","D-pad up","ArrowUp"],["dpadDown","D-pad down","ArrowDown"],["dpadLeft","D-pad left","ArrowLeft"],["dpadRight","D-pad right","ArrowRight"],
  ["a","A","Space"],["b","B","KeyC"],["x","X","KeyR"],["y","Y","KeyF"],["l","L","KeyQ"],["r","R","KeyE"],["zl","ZL","ShiftLeft"],["zr","ZR","ControlLeft"],
  ["minus","Minus","Backspace"],["plus","Plus","Enter"],["lclick","Left stick click","KeyZ"],["rclick","Right stick click","KeyX"],["home","Home","KeyH"],["capture","Capture","KeyP"]
];
const actionBits={a:4,b:2,x:8,y:1,l:16,r:32,zl:64,zr:128,minus:256,plus:512,lclick:1024,rclick:2048,home:4096,capture:8192};
const defaultDesktopMapping=()=>({keys:Object.fromEntries(mappingActions.map(([id,,code])=>[id,code])),mouseLeft:"zr",mouseRight:"zl",mouseAxis:"right",sensitivity:5.2});
let desktopMapping=defaultDesktopMapping(),captureAction="";
function loadDesktopMapping(){try{const current=localStorage.getItem(mappingKey),saved=JSON.parse(current||localStorage.getItem(legacyMappingKey)||"null");if(saved){desktopMapping={...defaultDesktopMapping(),...saved,keys:{...defaultDesktopMapping().keys,...saved.keys}};if(!current&&desktopMapping.sensitivity===2.8)desktopMapping.sensitivity=5.2;if(!current)saveDesktopMapping()}}catch(e){desktopMapping=defaultDesktopMapping()}}
function saveDesktopMapping(){try{localStorage.setItem(mappingKey,JSON.stringify(desktopMapping))}catch(e){}}
function keyLabel(code){return code.replace(/^Key/,"").replace(/^Digit/,"").replace("Arrow","").replace("Left"," L").replace("Right"," R")}
function actionOptions(selected){return `<option value="">None</option>${Object.keys(actionBits).map(id=>`<option value="${id}"${id===selected?" selected":""}>${id.toUpperCase()}</option>`).join("")}`}
function updateMappingHints(){
  const hint=id=>{const labels=[keyLabel(desktopMapping.keys[id])];if(desktopMapping.mouseLeft===id)labels.push("M1");if(desktopMapping.mouseRight===id)labels.push("M2");return labels.join(" / ")};
  for(const [id,bit] of Object.entries(actionBits)){const button=document.querySelector(`button[data-bit="${bit}"]`);if(button)button.dataset.keyHint=hint(id)}
  for(const [id,dir] of [["dpadUp","up"],["dpadDown","down"],["dpadLeft","left"],["dpadRight","right"]])document.querySelector(`button[data-dir="${dir}"]`).dataset.keyHint=keyLabel(desktopMapping.keys[id]);
  const leftKeys=["leftUp","leftLeft","leftDown","leftRight"].map(id=>keyLabel(desktopMapping.keys[id])).join(" ");
  document.getElementById("leftStickIndicator").dataset.keyHint=`${leftKeys}${desktopMapping.mouseAxis==="left"?" + MOUSE":""}`;
  document.getElementById("rightStickIndicator").dataset.keyHint=desktopMapping.mouseAxis==="right"?"MOUSE":"UNMAPPED";
}
function renderKeymap(){
  const grid=document.getElementById("keymapGrid");grid.innerHTML=mappingActions.map(([id,label])=>`<label class="keymap-row"><span>${label}</span><button data-map-key="${id}">${keyLabel(desktopMapping.keys[id])}</button></label>`).join("")+`<label class="keymap-row"><span>Mouse left</span><select id="mouseLeftMap">${actionOptions(desktopMapping.mouseLeft)}</select></label><label class="keymap-row"><span>Mouse right</span><select id="mouseRightMap">${actionOptions(desktopMapping.mouseRight)}</select></label><label class="keymap-row"><span>Mouse movement</span><select id="mouseAxisMap"><option value="right"${desktopMapping.mouseAxis==="right"?" selected":""}>Right stick</option><option value="left"${desktopMapping.mouseAxis==="left"?" selected":""}>Left stick</option><option value="off"${desktopMapping.mouseAxis==="off"?" selected":""}>Off</option></select></label><label class="keymap-row"><span>Mouse sensitivity</span><input id="mouseSensitivity" type="range" min="1" max="12" step="0.2" value="${desktopMapping.sensitivity}"></label>`;
  for(const button of grid.querySelectorAll("[data-map-key]"))button.addEventListener("click",()=>{captureAction=button.dataset.mapKey;button.textContent="Press key";button.classList.add("capturing")});
  for(const [id,key] of [["mouseLeftMap","mouseLeft"],["mouseRightMap","mouseRight"],["mouseAxisMap","mouseAxis"],["mouseSensitivity","sensitivity"]])document.getElementById(id).addEventListener("input",e=>{desktopMapping[key]=key==="sensitivity"?Number(e.target.value):e.target.value;saveDesktopMapping();updateMappingHints()});
}
loadDesktopMapping();updateMappingHints();
function pointerLocked(){return (document.pointerLockElement||document.webkitPointerLockElement)===gamepadEl}
function syncDesktopVisuals(){
  const active=desktopMode;
  for(const button of document.querySelectorAll("button[data-bit]"))button.classList.toggle("desktop-lit",active&&!!(desktop.buttons&Number(button.dataset.bit)));
  const hats={up:[0,1,7],right:[1,2,3],down:[3,4,5],left:[5,6,7]};
  for(const button of document.querySelectorAll("button[data-dir]"))button.classList.toggle("desktop-lit",active&&hats[button.dataset.dir].includes(desktop.hat));
  for(const [id,x,y,bit] of [["stickL","lx","ly",1024],["stickR","rx","ry",2048]]){
    const stick=document.getElementById(id),nub=stick.querySelector(".nub"),max=stick.getBoundingClientRect().width*.31;
    const dx=(desktop[x]-128)/127*max,dy=(desktop[y]-128)/127*max;
    nub.style.transform=active?`translate(calc(-50% + ${dx}px),calc(-50% + ${dy}px))`:"translate(-50%,-50%)";
    stick.classList.toggle("desktop-click",active&&!!(desktop.buttons&bit));
  }
}
function updateDesktopInput(urgent=false){
  let buttons=0;for(const [id,bit] of Object.entries(actionBits))if(desktopKeys.has(desktopMapping.keys[id]))buttons|=bit;if(desktopMouseButtons.has(0))buttons|=actionBits[desktopMapping.mouseLeft]||0;if(desktopMouseButtons.has(2))buttons|=actionBits[desktopMapping.mouseRight]||0;desktop.buttons=buttons;
  const left=desktopKeys.has(desktopMapping.keys.leftLeft),right=desktopKeys.has(desktopMapping.keys.leftRight),up=desktopKeys.has(desktopMapping.keys.leftUp),down=desktopKeys.has(desktopMapping.keys.leftDown);desktopAxes.lx=left||right||up||down;desktop.lx=128+(right?127:0)-(left?127:0);desktop.ly=128+(down?127:0)-(up?127:0);
  const du=desktopKeys.has(desktopMapping.keys.dpadUp),dd=desktopKeys.has(desktopMapping.keys.dpadDown),dl=desktopKeys.has(desktopMapping.keys.dpadLeft),dr=desktopKeys.has(desktopMapping.keys.dpadRight);desktop.hat=du&&dr?1:dr&&dd?3:dd&&dl?5:dl&&du?7:du?0:dr?2:dd?4:dl?6:8;syncDesktopVisuals();send(urgent);
}
function clearDesktopInput(){desktopKeys.clear();desktopMouseButtons.clear();mouseVX=mouseVY=0;Object.assign(desktop,neutral());desktopAxes.lx=desktopAxes.rx=false;syncDesktopVisuals();send(true)}
async function enterDesktopMode(){
  if(editing)return;
  try{const requestFullscreen=gamepadEl.requestFullscreen||gamepadEl.webkitRequestFullscreen;if(requestFullscreen){const result=requestFullscreen.call(gamepadEl);if(result&&result.then)await result}desktopMode=true;document.body.classList.add("desktop-mode");const requestLock=gamepadEl.requestPointerLock||gamepadEl.webkitRequestPointerLock;if(requestLock){const result=requestLock.call(gamepadEl);if(result&&result.catch)result.catch(()=>{})}}catch(e){statusEl.textContent="fullscreen unavailable"}
}
desktopButton.addEventListener("click",enterDesktopMode);
document.getElementById("editKeymap").addEventListener("click",()=>{updateMappingHints();renderKeymap();document.getElementById("keymapPanel").classList.add("open")});
document.getElementById("closeKeymap").addEventListener("click",()=>{captureAction="";document.getElementById("keymapPanel").classList.remove("open")});
document.getElementById("resetKeymap").addEventListener("click",()=>{desktopMapping=defaultDesktopMapping();saveDesktopMapping();updateMappingHints();renderKeymap()});
document.addEventListener("fullscreenchange",()=>{desktopMode=document.fullscreenElement===gamepadEl;document.body.classList.toggle("desktop-mode",desktopMode);if(!desktopMode)clearDesktopInput()});
document.addEventListener("webkitfullscreenchange",()=>{desktopMode=document.webkitFullscreenElement===gamepadEl;document.body.classList.toggle("desktop-mode",desktopMode);if(!desktopMode)clearDesktopInput()});
document.addEventListener("pointerlockchange",()=>{if(!pointerLocked()){
  mouseVX=mouseVY=0;
  if(desktopMapping.mouseAxis==="left"){desktop.lx=desktop.ly=128;desktopAxes.lx=false}
  else{desktop.rx=desktop.ry=128;desktopAxes.rx=false}
  updateDesktopInput()
}});
document.addEventListener("keydown",e=>{if(captureAction){e.preventDefault();desktopMapping.keys[captureAction]=e.code;captureAction="";saveDesktopMapping();updateMappingHints();renderKeymap();return}if(!desktopMode||!Object.values(desktopMapping.keys).includes(e.code)||e.target===photoInput)return;e.preventDefault();if(e.repeat)return;desktopKeys.add(e.code);updateDesktopInput(true)});
document.addEventListener("keyup",e=>{if(!Object.values(desktopMapping.keys).includes(e.code))return;if(desktopMode)e.preventDefault();desktopKeys.delete(e.code);updateDesktopInput(true)});
document.addEventListener("mousemove",e=>{if(!desktopMode||!pointerLocked()||desktopMapping.mouseAxis==="off")return;mouseVX=Math.max(-127,Math.min(127,mouseVX+e.movementX*desktopMapping.sensitivity));mouseVY=Math.max(-127,Math.min(127,mouseVY+e.movementY*desktopMapping.sensitivity));const left=desktopMapping.mouseAxis==="left";desktopAxes[left?"lx":"rx"]=true;desktop[left?"lx":"rx"]=Math.round(128+mouseVX);desktop[left?"ly":"ry"]=Math.round(128+mouseVY);syncDesktopVisuals();send()});
document.addEventListener("mousedown",e=>{if(!desktopMode||!pointerLocked())return;e.preventDefault();e.stopPropagation();desktopMouseButtons.add(e.button);updateDesktopInput(true)},true);
document.addEventListener("mouseup",e=>{if(!desktopMode)return;e.preventDefault();desktopMouseButtons.delete(e.button);updateDesktopInput(true)},true);
window.addEventListener("blur",clearDesktopInput);
function desktopFrame(){const left=desktopMapping.mouseAxis==="left",axis=left?"lx":"rx";if(desktopMode&&pointerLocked()&&desktopAxes[axis]){mouseVX*=.82;mouseVY*=.82;if(Math.abs(mouseVX)<.7)mouseVX=0;if(Math.abs(mouseVY)<.7)mouseVY=0;desktop[axis]=Math.round(128+mouseVX);desktop[left?"ly":"ry"]=Math.round(128+mouseVY);desktopAxes[axis]=mouseVX!==0||mouseVY!==0;syncDesktopVisuals();send()}requestAnimationFrame(desktopFrame)}desktopFrame();
function pressButton(el,on){
  const bit=Number(el.dataset.bit);touch.buttons=on?touch.buttons|bit:touch.buttons&~bit;el.classList.toggle("down",on);send(true);
  if(on&&navigator.vibrate)navigator.vibrate(8);
}
for(const b of document.querySelectorAll("button[data-bit]")){
  let lastPointer=0;
  b.addEventListener("pointerdown",e=>{if(editing)return;e.preventDefault();lastPointer=performance.now();b.setPointerCapture(e.pointerId);pressButton(b,true)});
  const off=e=>{if(editing)return;e.preventDefault();pressButton(b,false)};
  b.addEventListener("pointerup",off);b.addEventListener("pointercancel",off);b.addEventListener("lostpointercapture",()=>pressButton(b,false));
  b.addEventListener("click",()=>{if(editing||performance.now()-lastPointer<500)return;pressButton(b,true);setTimeout(()=>pressButton(b,false),85)});
}
function updateHat(){
  const u=dirs.has("up"),d=dirs.has("down"),l=dirs.has("left"),r=dirs.has("right");
  touch.hat=u&&r?1:r&&d?3:d&&l?5:l&&u?7:u?0:r?2:d?4:l?6:8;send(true);
}
for(const b of document.querySelectorAll("button[data-dir]")){
  let lastPointer=0;
  b.addEventListener("pointerdown",e=>{if(editing)return;e.preventDefault();lastPointer=performance.now();b.setPointerCapture(e.pointerId);dirs.add(b.dataset.dir);b.classList.add("down");updateHat()});
  const off=e=>{if(editing)return;e.preventDefault();dirs.delete(b.dataset.dir);b.classList.remove("down");updateHat()};
  b.addEventListener("pointerup",off);b.addEventListener("pointercancel",off);b.addEventListener("lostpointercapture",off);
  b.addEventListener("click",()=>{if(editing||performance.now()-lastPointer<500)return;dirs.add(b.dataset.dir);b.classList.add("down");updateHat();setTimeout(()=>{dirs.delete(b.dataset.dir);b.classList.remove("down");updateHat()},85)});
}
for(const zone of document.querySelectorAll(".side")){
  const stick=zone.querySelector(".stick"),nub=stick.querySelector(".nub"),isLeft=zone.classList.contains("left");let pointer=null,baseX=0,baseY=0,x="lx",y="ly";
  const chooseAxes=()=>{const rightStick=isLeft?sticksSwapped:!sticksSwapped;x=rightStick?"rx":"lx";y=rightStick?"ry":"ly"};
  const move=e=>{const max=stick.getBoundingClientRect().width*.31;let dx=e.clientX-baseX,dy=e.clientY-baseY,d=Math.hypot(dx,dy);if(d>max){dx=dx/d*max;dy=dy/d*max}nub.style.transform=`translate(calc(-50% + ${dx}px),calc(-50% + ${dy}px))`;const outputMax=max*.72;touch[x]=Math.round(Math.max(1,Math.min(255,128+dx/outputMax*127)));touch[y]=Math.round(Math.max(1,Math.min(255,128+dy/outputMax*127)));send()};
  zone.addEventListener("pointerdown",e=>{if(editing||pointer!==null||e.target.closest("button,.dpad,.face,.shoulders"))return;e.preventDefault();chooseAxes();pointer=e.pointerId;baseX=e.clientX;baseY=e.clientY;stick.style.left=`${baseX}px`;stick.style.top=`${baseY}px`;stick.classList.add("active");activeSticks[x]=true;zone.setPointerCapture(pointer);move(e)});
  zone.addEventListener("pointermove",e=>{if(e.pointerId===pointer)move(e)});
  const end=e=>{if(e.pointerId!==pointer)return;pointer=null;activeSticks[x]=false;stick.classList.remove("active");nub.style.transform="translate(-50%,-50%)";touch[x]=128;touch[y]=128;send()};
  zone.addEventListener("pointerup",end);zone.addEventListener("pointercancel",end);zone.addEventListener("lostpointercapture",end);
}
const movable=[...document.querySelectorAll("[data-move]")];
const layoutKey=()=>`switchpad-layout-v1-${matchMedia("(orientation:portrait)").matches?"portrait":"landscape"}`;
function setOffset(el,x,y){el.dataset.moveX=String(Math.round(x));el.dataset.moveY=String(Math.round(y));el.style.translate=`${Math.round(x)}px ${Math.round(y)}px`}
function setSize(el,w,h){if(w){el.dataset.sizeW=String(Math.round(w));el.style.width=`${Math.round(w)}px`}else{delete el.dataset.sizeW;el.style.removeProperty("width")}if(h){el.dataset.sizeH=String(Math.round(h));el.style.height=`${Math.round(h)}px`}else{delete el.dataset.sizeH;el.style.removeProperty("height")}}
function setStickSwap(value){sticksSwapped=!!value;document.body.classList.toggle("sticks-swapped",sticksSwapped);swapButton.classList.toggle("active",sticksSwapped);document.getElementById("leftStickIndicator").textContent=sticksSwapped?"R":"L";document.getElementById("rightStickIndicator").textContent=sticksSwapped?"L":"R"}
function readLayout(){const layout=Object.fromEntries(movable.map(el=>[el.dataset.move,{x:Number(el.dataset.moveX)||0,y:Number(el.dataset.moveY)||0,w:Number(el.dataset.sizeW)||0,h:Number(el.dataset.sizeH)||0}]));layout.__sticksSwapped=sticksSwapped;return layout}
function applyLayout(layout={}){for(const el of movable){const p=layout[el.dataset.move]||{x:0,y:0,w:0,h:0};setOffset(el,p.x,p.y);setSize(el,p.w,p.h)}setStickSwap(layout.__sticksSwapped);positionResizeHandle()}
function loadLayout(){try{applyLayout(JSON.parse(localStorage.getItem(layoutKey())||"{}"))}catch(e){applyLayout()}}
const resizeHandle=document.getElementById("resizeHandle");
const swapButton=document.getElementById("swapSticks");
const undoButton=document.getElementById("undoLayout");
let editSnapshot={},undoHistory=[],selected=null;
function updateUndo(){undoButton.disabled=undoHistory.length===0}
function pushUndo(){undoHistory.push(readLayout());if(undoHistory.length>30)undoHistory.shift();updateUndo()}
function selectControl(el){if(selected)selected.classList.remove("selected");selected=el;if(selected)selected.classList.add("selected");positionResizeHandle()}
function positionResizeHandle(){if(!editing||!selected){resizeHandle.classList.remove("active");return}const r=selected.getBoundingClientRect();resizeHandle.style.left=`${r.right-15}px`;resizeHandle.style.top=`${r.bottom-15}px`;resizeHandle.classList.add("active")}
function finishEdit(){selectControl(null);captureAction="";document.getElementById("keymapPanel").classList.remove("open");editing=false;document.body.classList.remove("editing")}
document.getElementById("editLayout").addEventListener("click",()=>{editSnapshot=readLayout();undoHistory=[];updateUndo();Object.assign(touch,neutral());dirs.clear();send(true);editing=true;document.body.classList.add("editing");selectControl(document.querySelector('[data-move="face"]'))});
swapButton.addEventListener("click",()=>{pushUndo();setStickSwap(!sticksSwapped)});
undoButton.addEventListener("click",()=>{if(!undoHistory.length)return;applyLayout(undoHistory.pop());updateUndo()});
document.getElementById("resetLayout").addEventListener("click",()=>{pushUndo();applyLayout()});
document.getElementById("cancelLayout").addEventListener("click",()=>{applyLayout(editSnapshot);finishEdit()});
document.getElementById("saveLayout").addEventListener("click",()=>{try{localStorage.setItem(layoutKey(),JSON.stringify(readLayout()))}catch(e){}finishEdit()});
for(const el of movable){
  let pointer=null,startX=0,startY=0,originX=0,originY=0,changed=false;
  el.addEventListener("pointerdown",e=>{if(!editing||pointer!==null)return;e.preventDefault();e.stopPropagation();selectControl(el);pointer=e.pointerId;changed=false;startX=e.clientX;startY=e.clientY;originX=Number(el.dataset.moveX)||0;originY=Number(el.dataset.moveY)||0;el.setPointerCapture(pointer)});
  el.addEventListener("pointermove",e=>{if(e.pointerId!==pointer)return;if(!changed){pushUndo();changed=true}let x=originX+e.clientX-startX,y=originY+e.clientY-startY;setOffset(el,x,y);const r=el.getBoundingClientRect(),pad=6;if(r.left<pad)x+=pad-r.left;if(r.right>innerWidth-pad)x-=r.right-(innerWidth-pad);if(r.top<pad)y+=pad-r.top;if(r.bottom>innerHeight-pad)y-=r.bottom-(innerHeight-pad);setOffset(el,x,y);positionResizeHandle()});
  const end=e=>{if(e.pointerId===pointer)pointer=null};el.addEventListener("pointerup",end);el.addEventListener("pointercancel",end);el.addEventListener("lostpointercapture",end);
}
{
  let pointer=null,startX=0,startY=0,startW=0,startH=0,changed=false;
  resizeHandle.addEventListener("pointerdown",e=>{if(!editing||!selected)return;e.preventDefault();e.stopPropagation();pointer=e.pointerId;changed=false;startX=e.clientX;startY=e.clientY;const r=selected.getBoundingClientRect();startW=r.width;startH=r.height;resizeHandle.setPointerCapture(pointer)});
  resizeHandle.addEventListener("pointermove",e=>{if(e.pointerId!==pointer||!selected)return;if(!changed){pushUndo();changed=true}setSize(selected,Math.max(30,startW+e.clientX-startX),Math.max(30,startH+e.clientY-startY));positionResizeHandle()});
  const end=e=>{if(e.pointerId===pointer)pointer=null};resizeHandle.addEventListener("pointerup",end);resizeHandle.addEventListener("pointercancel",end);resizeHandle.addEventListener("lostpointercapture",end);
}
updateUndo();
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
  }else gpEl.textContent=desktopMode?"keyboard + mouse":"touch controls";
  send();requestAnimationFrame(pollGamepads);
}
window.addEventListener("gamepadconnected",()=>send(true));window.addEventListener("gamepaddisconnected",()=>send(true));pollGamepads();setInterval(()=>send(true),250);
document.addEventListener("contextmenu",e=>e.preventDefault());
document.addEventListener("dblclick",e=>e.preventDefault(),{passive:false});
document.addEventListener("gesturestart",e=>e.preventDefault(),{passive:false});
let lastTouchEnd=0;document.addEventListener("touchend",e=>{const now=Date.now();if(now-lastTouchEnd<320)e.preventDefault();lastTouchEnd=now},{passive:false});
</script>
</body>
</html>
)HTML";
