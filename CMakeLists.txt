cmake_minimum_required(VERSION 3.22)
project(sandbox C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

pico_sdk_init()

add_compile_options(-Wall)

add_subdirectory(drivers/fatfs)
add_subdirectory(drivers/sdcard)
add_subdirectory(drivers/ps2kbd)
add_subdirectory(drivers/vga)


include_directories(${CMAKE_CURRENT_LIST_DIR})

add_executable(sandbox
  main.cpp
)

target_compile_definitions(sandbox PRIVATE 
# VGA
VGA_GPIO_FIRST=6 
VGA_GPIO_OUTNUM=6 
VGA_GPIO_SYNC=12
VGA_VSYNC=13
# SDCARD
SDCARD_PIN_SPI0_CS=5
SDCARD_PIN_SPI0_SCK=2
SDCARD_PIN_SPI0_MOSI=3
SDCARD_PIN_SPI0_MISO=4
# PS2KBD
USE_PS2_KBD    
PS2KBD_GPIO_FIRST=0
)

# Pull in our pico_stdlib which aggregates commonly used features
if(PICO_ON_DEVICE)
  target_link_libraries(sandbox PRIVATE
  tinyusb_host
  tinyusb_board

vga
sdcard
fatfs
ps2kbd

        pico_stdlib pico_stdio pico_bootrom pico_multicore pico_stdio  pico_multicore
        hardware_clocks hardware_pio hardware_vreg hardware_pio
        hardware_sync hardware_pll hardware_spi hardware_irq hardware_dma


#    pico_multicore
#    pico_stdlib

#    hardware_timer
#    hardware_vreg
#    hardware_dma
#    hardware_pio
#    hardware_pwm

)
else()  
# need SDL2
  find_package(SDL2 REQUIRED)

  target_include_directories(
    sandbox
    INTERFACE ${CMAKE_CURRENT_LIST_DIR}
              # have both forms of these because windows doesn't have SDL2
              # prefix
              ${SDL2_INCLUDE_DIR}
              ${SDL2_INCLUDE_DIR}/SDL2)

  target_link_libraries(sandbox ${SDL2_LIBRARIES} pico_multicore pico_stdlib)
endif()

# enable for on pico debugging
pico_enable_stdio_usb(sandbox 0)
pico_enable_stdio_uart(sandbox 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(sandbox)
