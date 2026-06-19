# Four HID interface experiment

This diagnostic firmware exposes four independent Pokken-compatible HID interfaces from one ESP32-S3 USB device. Its small page at `http://192.168.0.107/` sends test buttons to each interface separately.

The build uses ESP-IDF 5.2 plus Espressif's `esp_tinyusb` and `tinyusb` components. Set `SWITCHPAD_ESP_USB_ROOT` to an `espressif/esp-usb` checkout. Put an `espressif/tinyusb` release/v0.17 checkout in a folder named `tinyusb`, then set `SWITCHPAD_TINYUSB_PARENT` to its parent before running `idf.py build`.

This is intentionally isolated from the stable Arduino firmware on `main`.
