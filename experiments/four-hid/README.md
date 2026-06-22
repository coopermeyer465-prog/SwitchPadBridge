# Four-controller SwitchPad bridge

This firmware exposes four independent Pokken-compatible HID interfaces from one ESP32-S3 USB device and serves the complete SwitchPad controller at `http://192.168.0.107/`.

Each browser installation gets a persistent local device ID. The ESP32 assigns active devices to P1-P4, keeps reconnecting devices on their existing slot, and returns a clear busy response when all four slots are occupied. An inactive slot becomes available after 15 seconds. Inputs use WebSocket with an automatic HTTP fallback.

The Windows remote-play relay can also send the same input payloads to UDP port `7777`. Its fixed device IDs claim P1-P4 in order, and unchanged heartbeat packets keep those slots alive without adding duplicate USB reports.

The build uses ESP-IDF 5.2 plus Espressif's `esp_tinyusb` and `tinyusb` components. Set `SWITCHPAD_ESP_USB_ROOT` to an `espressif/esp-usb` checkout. Put an `espressif/tinyusb` release/v0.17 checkout in a folder named `tinyusb`, then set `SWITCHPAD_TINYUSB_PARENT` to its parent before running `idf.py build`.

## Flashing

On the development Mac, open a new Terminal and run:

```sh
ESP32 Flash
```

The command builds the latest firmware and sends it to the bridge over Wi-Fi at `192.168.0.107`. The OTA partition layout must first be installed with one USB bootloader flash; the same command detects that bootloader and performs the initial flash automatically. Later updates do not require the reset or boot buttons.

## Controller limit

The ESP32-S3 USB peripheral supplies five bidirectional endpoint pairs. Each Pokken-compatible controller interface consumes one pair, so this experiment intentionally exposes four controllers and leaves the remaining USB resources available to the device stack. Eight genuine independent controller interfaces cannot be exposed by one ESP32-S3 USB peripheral.

This remains isolated from the stable single-controller Arduino firmware on `main` while the four-controller behavior is tested on Nintendo Switch hardware.
