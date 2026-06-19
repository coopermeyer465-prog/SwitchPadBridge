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

The ESP32 hosts a responsive touch gamepad at `http://switchpad.local/`. The page can also read a PlayStation, Xbox, Joy-Con, Backbone, or other OS-supported standard controller paired with or connected to the iPhone or iPad through the browser Gamepad API. Input is forwarded over Wi-Fi and emitted through the ESP32-S3 native USB interface.

> [!IMPORTANT]
> This is prototype firmware. Its USB profile emulates a HORI Pokken wired controller, a known target for original Switch projects. Switch 2 acceptance must still be tested on real hardware.

## iPhone and iPad

The simplest setup needs no separate virtual-gamepad app:

1. Connect the iPhone or iPad to the same Wi-Fi as the ESP32.
2. Open `http://switchpad.local/` in Safari. If mDNS does not resolve, use the IP printed in the serial monitor.
3. Rotate to landscape for the widest thumb spacing. Touch and drag anywhere unused on the left or right half to place and operate that side's floating analog stick.
4. Use the on-screen controls, or pair a physical controller in iOS/iPadOS Bluetooth settings and press a button while the page is open.
5. Keep Safari in the foreground. iOS and iPadOS may suspend the page and its connection when you switch to another app or lock the device.

The layout responds to phone/iPad size and orientation. Touch and physical-controller state are merged, so either input method can be used without changing modes. Controller compatibility still depends on iOS/iPadOS and Safari exposing that device through the standard Gamepad API; unusual generic pads may use a nonstandard button order.

Tap the pencil in the lower-right corner to edit the layout. Drag the center buttons, shoulder groups, D-pad, or face-button group; Reset restores defaults, Cancel discards the current edit, and Save stores separate portrait and landscape layouts locally on that device.

A separate app only works if it sends input directly to the ESP32. The firmware currently accepts its compact input format over UDP port `7777`; OSC apps such as TouchOSC would require a small OSC parser or a custom bridge.

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
