# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 1.5.1)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Definir o board como Pico W
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

# Nome do projeto corrigido
project(led_control_webserver C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Nome do arquivo executável
add_executable(led_control_webserver led_control_webserver.c)

pico_set_program_name(led_control_webserver "led_control_webserver")
pico_set_program_version(led_control_webserver "1.0")

# Enable/disable output over UART/USB
pico_enable_stdio_uart(led_control_webserver 0)
pico_enable_stdio_usb(led_control_webserver 1)

# Add the standard libraries to the build
target_link_libraries(led_control_webserver
    pico_stdlib
    hardware_gpio
    hardware_adc
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mqtt
)

# Include directories
target_include_directories(led_control_webserver PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
)

# Create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(led_control_webserver)