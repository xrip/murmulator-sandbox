cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

include(pico_sdk_import.cmake)

project(SANDBOX C CXX ASM)

pico_sdk_init()

add_subdirectory(drivers/vga)
add_subdirectory(drivers/sdcard)
add_subdirectory(drivers/fatfs)
add_subdirectory(drivers/nespad)
add_subdirectory(drivers/ps2kbd)

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c" "src/*.h")

message(STATUS "")
message(STATUS "Add source files:")
foreach(SRC_FILE IN LISTS SRC)
    message(STATUS "${SRC_FILE}")
endforeach()
message(STATUS "")

add_executable(${PROJECT_NAME} ${SRC})

pico_set_program_name(${PROJECT_NAME} "MURMULATOR devboard Sandbox")
pico_set_program_version(${PROJECT_NAME} "v0.1")


pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 1)

target_link_libraries(${PROJECT_NAME} PRIVATE
		vga
		ps2kbd
		nespad
		sdcard
		fatfs

		pico_runtime
        pico_stdlib
        pico_multicore
        hardware_dma
        hardware_pio
        hardware_i2c
        hardware_interp
        hardware_timer
        hardware_clocks
        hardware_pwm
        hardware_flash
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
		# 16MB flash
		PICO_FLASH_SIZE_BYTES=16777216

        SPEAKER_ENABLED
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

        # NES Gamepad
        USE_NESPAD
        NES_GPIO_CLK=14
        NES_GPIO_DATA=16
        NES_GPIO_LAT=15

        #AUDIO
        AUDIO_PWM_PIN=26

        # Other
        FF_USE_FIND
)

pico_add_extra_outputs(${PROJECT_NAME})



