add_executable(blank_canvas main.c game.c input.c)

target_include_directories(blank_canvas PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(blank_canvas PRIVATE ili9488 font5x7 pico_stdlib)
target_compile_options(blank_canvas PRIVATE -Wall)

pico_set_program_name(blank_canvas "blank_canvas")
pico_set_program_description(blank_canvas "Blank canvas for a 76x284 ST7789 and RP2040 USB stick")
pico_set_program_version(blank_canvas "${PROJECT_VERSION}")

# USB serial for stdio and the 1200-baud reboot into BOOTSEL
pico_enable_stdio_usb(blank_canvas 1)
pico_enable_stdio_uart(blank_canvas 0)

# Produces blank_canvas.uf2 for drag-and-drop flashing
pico_add_extra_outputs(blank_canvas)
