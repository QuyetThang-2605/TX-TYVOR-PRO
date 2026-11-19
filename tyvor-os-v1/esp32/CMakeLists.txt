cmake_minimum_required(VERSION 3.16)
set(IDF_TARGET "esp32s3")
set(PROJECT_VER "0.2.2-timing-bench")
set(COMPONENTS main) # UART/UI-only dependency closure; no Wi-Fi/Bluetooth firmware.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tyvor_display)
