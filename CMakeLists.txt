cmake_minimum_required(VERSION 3.13)

# Default board is the RP2350 (pico2). Override with -DPICO_BOARD=rp2040_usb_stick for the
# "RP2040 USB" stick (boards/rp2040_usb_stick.h, 4 MB flash), or -DPICO_BOARD=pico for a Pico.
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)
set(PICO_BOARD pico2 CACHE STRING "Pico SDK board")

# Must come before project()
include(cmake/pico_sdk_import.cmake)

project(blank_canvas VERSION 1.0.0 LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(lib/font5x7)
add_subdirectory(lib/ili9488)
add_subdirectory(src)
