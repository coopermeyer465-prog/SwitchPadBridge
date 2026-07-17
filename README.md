# SwitchPadBridge

Turn a Seeed Studio XIAO ESP32-S3 into a Wi-Fi-to-USB controller bridge for Nintendo Switch and Switch 2 experimentation.

```text
iPhone, iPad, computer, or paired Bluetooth controller
                 |
              Wi-Fi
                 |
          XIAO ESP32-S3
                 |
              USB HID
                 |
       Nintendo Switch / Switch 2
```

The ESP32 hosts a responsive touch gamepad at `http://switchpad.local/`. The page can also read a PlayStation, Xbox, Joy-Con, Backbone, or other OS-supported standard controller paired with or connected to the phone, tablet, or computer through the browser Gamepad API. Input is forwarded over Wi-Fi and emitted through the ESP32-S3 native USB interface.

> [!IMPORTANT]
> This is prototype firmware. Its USB profile emulates HORI Pokken wired controllers, a known target for original Switch projects. Switch 2 acceptance must still be tested on real hardware.

## iPhone and iPad

The simplest setup needs no separate virtual-gamepad app:

1. Connect the iPhone or iPad to the same Wi-Fi as the ESP32.
2. Open `http://switchpad.local/` in Safari.
3. Rotate to landscape for the widest thumb spacing. Touch and drag anywhere unused on the left or right half to place and operate that side's floating analog stick.
4. Use the on-screen controls, or pair a physical controller in iOS/iPadOS Bluetooth settings and press a button while the page is open.
5. Keep Safari in the foreground. iOS and iPadOS may suspend the page and its connection when you switch to another app or lock the device.

The layout responds to phone/iPad size and orientation. Touch and physical-controller state are merged, so either input method can be used without changing modes. Controller compatibility still depends on iOS/iPadOS and Safari exposing that device through the standard Gamepad API; unusual generic pads may use a nonstandard button order.

Tap the pencil in the lower-right corner to edit the layout. Every button can be moved and resized independently, while the utility, shoulder, and face groups can still be moved as units. The selected item has a turquoise corner handle that changes its width and height. The crossed-arrows control swaps which side drives the left and right analog sticks; L/R indicators appear at the original fixed-stick locations while editing. The photo control chooses a full-viewport background image, which is compressed and stored only on that phone or iPad, and the adjacent trash control removes it. Undo reverses moves, resizes, swaps, and Reset actions one step at a time; Cancel discards the entire layout edit, and Save stores separate portrait and landscape layouts locally on that device. Short iPhone landscape screens use a more compact layout while iPad sizing remains unchanged.

## Multi-player Status

This branch exposes four separate Pokken-compatible HID interfaces from one ESP32-S3. Each browser device gets a persistent local device ID, and the firmware assigns active devices to player slots P1 through P4. Reconnecting devices keep their slot when possible, and inactive slots are released after a short timeout.

The ESP32-S3 cannot expose eight genuine USB controller interfaces with this profile because its native USB peripheral has a limited endpoint budget. Four interfaces is the practical target for this one-board experiment. Smash Bros. can support more players, but going beyond four would require different hardware or an external USB architecture.

A separate app only works if it sends input directly to the ESP32. The firmware accepts the compact input format over UDP port `7777`; OSC apps such as TouchOSC would require a small OSC parser or a custom bridge.

The project also includes a [Windows remote-play relay](host/windows-relay/README.md). It reads up to four XInput controllers created by Sunshine for Moonlight clients and forwards them over the LAN to the four USB controller interfaces on the Switch. A capture card is not required to test this input path.

## Keyboard and mouse

On a computer, click the fullscreen icon in the lower-left corner to enter desktop controller mode. The page uses pointer lock so mouse motion drives the right stick and automatically springs back to center. Press Escape to release the mouse and leave fullscreen.

While editing on a computer, use the keyboard icon to change every key binding, assign left and right mouse clicks, choose which stick mouse movement controls, and adjust mouse sensitivity. These mappings are saved locally on that computer. Desktop input highlights its mapped on-screen buttons, and the fixed stick graphics mirror WASD and mouse movement. On-screen controls respond immediately to mouse-down while still supporting completed and keyboard-generated clicks; touch controls respond immediately to touch-down for gameplay.

| Input | Switch control |
| --- | --- |
| WASD | Left stick |
| Mouse | Right stick |
| Arrow keys | D-pad |
| Space / C / R / F | A / B / X / Y |
| Left / right mouse | ZR / ZL |
| Q / E | L / R |
| Shift / Control | ZL / ZR |
| Enter / Backspace | Plus / Minus |
| H / P | Home / Capture |
| Z / X | Left / right stick click |

J/K/U/I are alternate B/A/Y/X bindings for emulator-style keyboard layouts. Keyboard mappings activate only inside desktop controller mode, so the page does not capture normal typing outside fullscreen.

## Build And Flash

### Four-HID ESP-IDF Firmware

This branch's multi-controller firmware lives in `experiments/four-hid`.

To put it on a XIAO ESP32-S3:

1. Clone the project and enter the repo:

   ```sh
   git clone https://github.com/coopermeyer465-prog/SwitchPadBridge.git
   cd SwitchPadBridge
   git checkout experiment/four-hid-interfaces
   ```

2. Create local Wi-Fi credentials:

   ```sh
   cp secrets.h.example secrets.h
   ```

   Edit `secrets.h` and set `WIFI_SSID` and `WIFI_PASSWORD` to the Wi-Fi network the phone, tablet, or computer will use. This file is ignored by Git so private Wi-Fi passwords do not get committed.

3. Install ESP-IDF 5.2 and the local component dependencies used by `scripts/esp32-flash.sh`. On the development Mac for this repo, the helper expects ESP-IDF at `~/esp/esp-idf` and will fetch Espressif's USB, TinyUSB, and mDNS dependencies if they are missing.

4. Put the XIAO ESP32-S3 in bootloader mode. On a working reset button, double-press reset. If the reset button is damaged, briefly short the reset pads according to the XIAO ESP32-S3 hardware docs. The board should appear as `/dev/cu.usbmodem*` on macOS.

5. Flash the firmware:

   ```sh
   ESP32 Flash
   ```

   If the `ESP32 Flash` shell shortcut is not installed, run the helper directly:

   ```sh
   ./scripts/esp32-flash.sh
   ```

6. After the ESP32 reboots and joins Wi-Fi, open:

   ```text
   http://switchpad.local/
   ```

   If mDNS does not resolve on a network, look up the ESP32's DHCP address in the router or with `arp -a`, then open `http://<esp32-ip>/`.

The helper builds the ESP-IDF firmware and attempts an OTA update at `http://switchpad.local/api/update`. If the bridge is not reachable over Wi-Fi, it falls back to a USB bootloader flash on `/dev/cu.usbmodem*`.

The firmware now uses DHCP instead of a fixed IP address and advertises itself as `switchpad.local` with mDNS. Local Wi-Fi credentials go in `secrets.h`, which is ignored by Git.

### Arduino IDE

The older Arduino firmware is still useful as a simpler single-controller reference:

1. Install the Espressif `esp32` board package.
2. Open `SwitchPadBridge.ino` and select `XIAO_ESP32S3`.
3. Set `USB Mode` to `USB-OTG (TinyUSB)`.
4. Use `USB CDC On Boot: Disabled` for the controller-only build used with a Switch. Enable it only for serial debugging on a computer.
5. Copy `secrets.h.example` to `secrets.h` and enter your Wi-Fi credentials. `secrets.h` is ignored by Git.
6. Put the board in bootloader mode and upload.

### Arduino CLI

```sh
cp secrets.h.example secrets.h
arduino-cli compile --fqbn 'esp32:esp32:XIAO_ESP32S3:USBMode=default,CDCOnBoot=cdc' .
arduino-cli upload -p /dev/cu.usbmodem101 --fqbn 'esp32:esp32:XIAO_ESP32S3:USBMode=default,CDCOnBoot=cdc' .
```

If no credentials are compiled in or saved, the ESP32 creates the open setup network `SwitchPadBridge-Setup`. Join it and open `http://192.168.4.1/` to save Wi-Fi settings.

## Input API

Every transport accepts the same URL-encoded payload:

```text
buttons=0x0004&hat=8&lx=128&ly=128&rx=128&ry=128
```

- WebSocket: `ws://<esp32-ip>/ws`
- HTTP POST: `http://<esp32-ip>/api/input`
- UDP: `<esp32-ip>:7777`
- Slot claim: `GET http://<esp32-ip>/api/claim?device=<device-id>`
- OTA update: `POST http://<esp32-ip>/api/update`

Stick values are `0..255`, centered at `128`. Hat values are `0..7`, with `8` neutral. Button bits are defined in `SwitchButton` near the top of the sketch.

## Troubleshooting

- If Safari loads and then reports that the server stopped responding, flash the latest firmware. Earlier versions could block forever on a partially delivered WebSocket frame.
- Confirm the ESP32 and phone are on the same non-guest Wi-Fi and that client isolation is disabled.
- Test `http://switchpad.local/` first. It should load the controller page.
- On the Switch, enable wired Pro Controller communication if that setting is available.
- The controller-only build has no USB serial port. Double-press the XIAO reset button to return to bootloader mode for the next upload.
- If a computer recognizes `HORI CO.,LTD. POKKEN CONTROLLER` but Switch 2 does not, the likely next task is updating the USB profile; the Wi-Fi input path can remain unchanged.

## License

MIT

## Built With Codex

Codex was used as the pair-programming agent for this project. It helped inspect the existing firmware, build the ESP-IDF four-HID experiment, tune the browser controller UI, add desktop keyboard/mouse support, add OTA flashing, switch the bridge from a fixed IP to DHCP plus `switchpad.local`, and repeatedly build, flash, test, commit, and push changes from the local workspace.

The project still depends on real hardware testing. Codex helped move quickly through firmware and UI iterations, but the Switch behavior, controller timing, Wi-Fi reliability, and product hardware choices all need hands-on validation.
