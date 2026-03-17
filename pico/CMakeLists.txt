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
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(hydra C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(hydra main.cpp
    log.cpp
    httpd.cpp
    websocket.cpp
    bt.cpp
    hid.cpp)

pico_set_program_name(hydra "hydra")
pico_set_program_version(hydra "2.0")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(hydra 1)
pico_enable_stdio_usb(hydra 0)

# Add the standard include files to the build. This is required as there are "dirty" includes in the SDK that assume this is done.
target_include_directories(hydra PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

pico_btstack_make_gatt_header(hydra PRIVATE "${CMAKE_CURRENT_LIST_DIR}/device.gatt")

# Add any user requested libraries
target_link_libraries(hydra 

        # standard lib
        pico_stdlib

        # arch_none means we're not using TCP/IP stack, just onboard LED (which is part of wifi driver)
        # pico_cyw43_arch_none

        # lwIP and HTTP support (if needed - can be removed if not using lwIP or HTTP)
        pico_cyw43_arch_lwip_threadsafe_background
        pico_lwip_http
        pico_lwip_mdns
        pico_httpd_content
        pico_stdlib

        # btstack
        pico_btstack_ble
        pico_btstack_cyw43

        # other
        hardware_adc
        hardware_flash
        hardware_sync

        )

pico_add_extra_outputs(hydra)

pico_add_library(pico_httpd_content NOFLAG)
pico_set_lwip_httpd_content(pico_httpd_content INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/content/404.html
        ${CMAKE_CURRENT_LIST_DIR}/content/index.html
    ${CMAKE_CURRENT_LIST_DIR}/content/icon.svg
        )

