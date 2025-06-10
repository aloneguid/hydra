# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(ipkm C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(ipkm main.cpp dashboard.cpp hid.cpp)

# pull in common dependencies
target_link_libraries(ipkm
    pico_stdlib
    pico_cyw43_arch_none
    pico_btstack_ble
    pico_btstack_cyw43
    hardware_adc)


target_include_directories(ipkm PRIVATE
    ${CMAKE_CURRENT_LIST_DIR} # For btstack config
    )

pico_btstack_make_gatt_header(ipkm PRIVATE "${CMAKE_CURRENT_LIST_DIR}/device.gatt")

# enable usb output, disable uart output
# "Bus Reported Device Description" will be "Board CDC" in Windows Device Manager.
pico_enable_stdio_usb(ipkm 1)
pico_enable_stdio_uart(ipkm 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(ipkm)