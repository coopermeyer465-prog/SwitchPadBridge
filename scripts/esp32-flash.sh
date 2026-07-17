#!/bin/zsh
set -euo pipefail

readonly PROJECT_DIR="/Users/marissameyer/Desktop/SwitchPadBridge/experiments/four-hid"
readonly IDF_DIR="/Users/marissameyer/esp/esp-idf"
readonly PY_ENV="/Users/marissameyer/.espressif/python_env/idf5.2_py3.13_env"
readonly ESP_USB_ROOT="/Users/marissameyer/esp/esp-usb"
readonly ESP_PROTOCOLS_ROOT="/Users/marissameyer/esp/esp-protocols"
readonly COMPONENT_PARENT="/Users/marissameyer/esp/switchpad-components"
readonly TINYUSB_DIR="$COMPONENT_PARENT/tinyusb"
readonly OTA_URL="http://switchpad.local/api/update"

install_components() {
  if [[ ! -d "$ESP_USB_ROOT/.git" ]]; then
    git clone https://github.com/espressif/esp-usb.git "$ESP_USB_ROOT"
    git -C "$ESP_USB_ROOT" checkout 8e779566ef71d43928cbf7e125e8eb54bab3f542
  fi
  if [[ ! -d "$TINYUSB_DIR/.git" ]]; then
    mkdir -p "$COMPONENT_PARENT"
    git clone --branch release/v0.17 https://github.com/espressif/tinyusb.git "$TINYUSB_DIR"
    git -C "$TINYUSB_DIR" checkout 82a7a644ccbe5a38c725517ca3baf0703f90b2fb
  fi
  if [[ ! -d "$ESP_PROTOCOLS_ROOT/.git" ]]; then
    git clone --depth 1 https://github.com/espressif/esp-protocols.git "$ESP_PROTOCOLS_ROOT"
  fi
}

install_components
export IDF_COMPONENT_MANAGER=0
export IDF_PYTHON_ENV_PATH="$PY_ENV"
export SWITCHPAD_ESP_USB_ROOT="$ESP_USB_ROOT"
export SWITCHPAD_ESP_PROTOCOLS_ROOT="$ESP_PROTOCOLS_ROOT"
export SWITCHPAD_TINYUSB_PARENT="$COMPONENT_PARENT"
source "$IDF_DIR/export.sh" >/dev/null

cd "$PROJECT_DIR"
idf.py build

if curl --fail --silent --show-error --connect-timeout 2 --max-time 45 \
  -H "Content-Type: application/octet-stream" \
  -H "X-SwitchPad-Update: local-firmware" \
  --data-binary @build/switchpad_four_hid.bin \
  "$OTA_URL"; then
  echo
  echo "SwitchPad updated over Wi-Fi and is rebooting."
  exit 0
fi

port=(/dev/cu.usbmodem*(N[1]))
if (( ${#port[@]} == 0 )); then
  echo "Wi-Fi update is not available yet and no USB bootloader was found."
  echo "Put the XIAO in bootloader once, then run: ESP32 Flash"
  exit 1
fi

echo "Wi-Fi update unavailable; flashing bootloader device at ${port[1]}."
idf.py -p "${port[1]}" flash
"$PY_ENV/bin/python" "$IDF_DIR/components/esptool_py/esptool/esptool.py" \
  --chip esp32s3 -p "${port[1]}" --after watchdog_reset run || true
echo "SwitchPad USB flash complete. Future updates can use Wi-Fi."
