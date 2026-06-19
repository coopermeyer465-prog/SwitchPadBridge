# Four-controller SwitchPad bridge

This firmware exposes four independent Pokken-compatible HID interfaces from one ESP32-S3 USB device and serves the complete SwitchPad controller at `http://192.168.0.107/`.

Each browser installation gets a persistent local device ID. The ESP32 assigns active devices to P1-P4, keeps reconnecting devices on their existing slot, and returns a clear busy response when all four slots are occupied. An inactive slot becomes available after 15 seconds. Inputs use WebSocket with an automatic HTTP fallback.

The build uses ESP-IDF 5.2 plus Espressif's `esp_tinyusb` and `tinyusb` components. Set `SWITCHPAD_ESP_USB_ROOT` to an `espressif/esp-usb` checkout. Put an `espressif/tinyusb` release/v0.17 checkout in a folder named `tinyusb`, then set `SWITCHPAD_TINYUSB_PARENT` to its parent before running `idf.py build`.

This remains isolated from the stable single-controller Arduino firmware on `main` while the four-controller behavior is tested on Nintendo Switch hardware.
