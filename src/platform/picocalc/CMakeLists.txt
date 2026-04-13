cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ========================================================================================

set(PICO_BOARD pico2 CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(OpenLara-PicoCalc C CXX ASM)

pico_sdk_init()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/picocalc/drivers/rp2040-psram rp2040-psram)

set(TARGET_NAME OpenLara-PicoCalc)

add_executable(${TARGET_NAME}
    #PsramTest.cpp
    main.cpp
    render.cpp
    sound.cpp
    LevelLoaderPico.cpp
    PkdStream.cpp

    picocalc/PicoCalcDisplay.cpp
    picocalc/PicoFileSystem.cpp
    picocalc/PicoInput.cpp
    picocalc/PicoSouthbridge.cpp
    picocalc/Psram.cpp

    picocalc/drivers/clib.c
    picocalc/drivers/fat32.c
    picocalc/drivers/sdcard.c
    picocalc/drivers/southbridge.c

    ../../fixed/common.cpp

    ../../libs/tinf/tinflate.c
    ../../libs/minimp3/minimp3.cpp
    ../../libs/stb_vorbis/stb_vorbis.c
)

pico_set_program_name(${TARGET_NAME} "OpenLara-PicoCalc")
pico_set_program_version(${TARGET_NAME} "0.1")

target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

target_compile_definitions(${TARGET_NAME} PRIVATE
    PICO_CLOCK_ADJUST_PERI_CLOCK_WITH_SYS_CLOCK=1
    __PICOCALC__
    #NO_SOUND=1

    PSRAM_MUTEX=1
    PSRAM_ASYNC=0
    PSRAM_PIN_CS=20
    PSRAM_PIN_SCK=21
    PSRAM_PIN_MOSI=2
    PSRAM_PIN_MISO=3

    $<$<CONFIG:Debug>:_DEBUG>
)

target_compile_options(${TARGET_NAME} PRIVATE
    -ffunction-sections
    -fdata-sections
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-enum-conversion>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-float-conversion>
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/picocalc
    ${CMAKE_CURRENT_LIST_DIR}/picocalc/drivers

    ${CMAKE_CURRENT_LIST_DIR}/../../fixed
    ${CMAKE_CURRENT_LIST_DIR}/../../fixed/fmt
    ${CMAKE_CURRENT_LIST_DIR}/../../fixed/lang
    ${CMAKE_CURRENT_LIST_DIR}/../../fixed/render
    ${CMAKE_CURRENT_LIST_DIR}/../../libs
)

target_link_libraries(${TARGET_NAME}
    pico_stdlib
    pico_sync
    pico_multicore
    hardware_spi
    hardware_gpio
    hardware_dma
    hardware_i2c
    hardware_interp
    hardware_pio
    hardware_sync
    hardware_timer
    hardware_clocks
    rp2040-psram
)

pico_enable_stdio_uart(${TARGET_NAME} 1)
pico_enable_stdio_usb(${TARGET_NAME} 0)

pico_add_extra_outputs(${TARGET_NAME})