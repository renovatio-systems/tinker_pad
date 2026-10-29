add_library(ili9488 STATIC
    src/ili9488.c
)

target_include_directories(ili9488 PUBLIC include)
target_link_libraries(ili9488 PUBLIC font5x7 pico_stdlib hardware_spi hardware_dma)
