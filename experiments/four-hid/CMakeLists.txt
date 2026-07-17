cmake_minimum_required(VERSION 3.16)

set(IDF_TARGET "esp32s3")
set(EXTRA_COMPONENT_DIRS
    "$ENV{SWITCHPAD_ESP_USB_ROOT}/device"
    "$ENV{SWITCHPAD_TINYUSB_PARENT}"
    "$ENV{SWITCHPAD_ESP_PROTOCOLS_ROOT}/components/mdns"
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(switchpad_four_hid)
