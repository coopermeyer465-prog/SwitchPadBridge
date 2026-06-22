# SwitchPadBridge

Turn a Seeed Studio XIAO ESP32-S3 into a Wi-Fi-to-USB controller bridge for Nintendo Switch and Switch 2 experimentation.

```text
iPhone, iPad, or paired Bluetooth controller
                 |
              Wi-Fi
                 |
          XIAO ESP32-S3
                 |
              USB HID
                 |
       Nintendo Switch / Switch 2
```

The ESP32 hosts a responsive touch gamepad at `http://192.168.0.107/` (and `http://switchpad.local/` where mDNS works). The page can also read a PlayStation, Xbox, Joy-Con, Backbone, or other OS-supported standard controller paired with or connected to the iPhone or iPad through the browser Gamepad API. Input is forwarded over Wi-Fi and emitted through the ESP32-S3 native USB interface.

> [!IMPORTANT]
> This is prototype firmware. Its USB profile emulates a HORI Pokken wired controller, a known target for original Switch projects. Switch 2 acceptance must still be tested on real hardware.

## iPhone and iPad

The simplest setup needs no separate virtual-gamepad app:

1. Connect the iPhone or iPad to the same Wi-Fi as the ESP32.
2. Open `http://192.168.0.107/` in Safari. `http://switchpad.local/` is also available when mDNS resolves correctly.
3. Rotate to landscape for the widest thumb spacing. Touch and drag anywhere unused on the left or right half to place and operate that side's floating analog stick.
4. Use the on-screen controls, or pair a physical controller in iOS/iPadOS Bluetooth settings and press a button while the page is open.
5. Keep Safari in the foreground. iOS and iPadOS may suspend the page and its connection when you switch to another app or lock the device.

The layout responds to phone/iPad size and orientation. Touch and physical-controller state are merged, so either input method can be used without changing modes. Controller compatibility still depends on iOS/iPadOS and Safari exposing that device through the standard Gamepad API; unusual generic pads may use a nonstandard button order.

Tap the pencil in the lower-right corner to edit the layout. Every button can be moved and resized independently, while the utility, shoulder, and face groups can still be moved as units. The selected item has a turquoise corner handle that changes its width and height. The crossed-arrows control swaps which side drives the left and right analog sticks; L/R indicators appear at the original fixed-stick locations while editing. The photo control chooses a full-viewport background image, which is compressed and stored only on that phone or iPad, and the adjacent trash control removes it. Undo reverses moves, resizes, swaps, and Reset actions one step at a time; Cancel discards the entire layout edit, and Save stores separate portrait and landscape layouts locally on that device. Short iPhone landscape screens use a more compact layout while iPad sizing remains unchanged.

## Multi-player status

The four-report-ID experiment was rejected by the Pokken controller profile and produced invalid button input. The stable firmware therefore exposes one USB controller, and multiple connected phones intentionally feed that same controller. A genuine multi-player version needs separate USB device addresses, which this XIAO cannot provide through its single USB device controller without additional hub/device hardware. TinyUSB's hub support is host-side, so a descriptor-only "virtual hub" cannot create working downstream controllers; the remaining one-board experiment is a custom composite device with multiple separate HID interfaces.

A separate app only works if it sends input directly to the ESP32. The firmware currently accepts its compact input format over UDP port `7777`; OSC apps such as TouchOSC would require a small OSC parser or a custom bridge.

The four-interface experiment includes a [Windows remote-play relay](host/windows-relay/README.md). It reads up to four XInput controllers created by Sunshine for Moonlight clients and forwards them over the LAN to four separate USB controller interfaces on the Switch. A capture card is not required to test this input path.

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

## Build and Flash

### Arduino IDE

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

- WebSocket: `ws://<esp32-ip>:81/ws`
- HTTP POST: `http://<esp32-ip>/api/input`
- UDP: `<esp32-ip>:7777`
- State: `GET http://<esp32-ip>/api/state`
- Health check: `GET http://<esp32-ip>/health`

Stick values are `0..255`, centered at `128`. Hat values are `0..7`, with `8` neutral. Button bits are defined in `SwitchButton` near the top of the sketch.

## Troubleshooting

- If Safari loads and then reports that the server stopped responding, flash the latest firmware. Earlier versions could block forever on a partially delivered WebSocket frame.
- Confirm the ESP32 and phone are on the same non-guest Wi-Fi and that client isolation is disabled.
- Test `/health` first. It should immediately return `ok`.
- On the Switch, enable wired Pro Controller communication if that setting is available.
- The controller-only build has no USB serial port. Double-press the XIAO reset button to return to bootloader mode for the next upload.
- If a computer recognizes `HORI CO.,LTD. POKKEN CONTROLLER` but Switch 2 does not, the likely next task is updating the USB profile; the Wi-Fi input path can remain unchanged.

## License

MIT
