add_executable(tinker_pad main.c game.c input.c led.c jatayu_img.c assets.c l2_sprites.c l3_assets.c l_labels.c l4_cutscenes.c)

pico_generate_pio_header(tinker_pad ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

target_include_directories(tinker_pad PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(tinker_pad PRIVATE ili9488 font5x7 pico_stdlib hardware_pio)
target_compile_options(tinker_pad PRIVATE -Wall)

pico_set_program_name(tinker_pad "tinker_pad")
pico_set_program_description(tinker_pad "Blank canvas for a 76x284 ST7789 and RP2040 USB stick")
pico_set_program_version(tinker_pad "${PROJECT_VERSION}")

# USB serial for stdio and the 1200-baud reboot into BOOTSEL
pico_enable_stdio_usb(tinker_pad 1)
# UART stdio is off: GP0 and GP1 are button inputs, and the 1200-baud reset uses USB serial

# Produces tinker_pad.uf2 for drag-and-drop flashing
pico_add_extra_outputs(tinker_pad)
