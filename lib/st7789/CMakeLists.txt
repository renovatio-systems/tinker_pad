add_library(st7789 STATIC
    src/st7789.c
)

target_include_directories(st7789 PUBLIC include)
target_link_libraries(st7789 PUBLIC pico_stdlib hardware_spi hardware_dma)
