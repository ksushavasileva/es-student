
cmake_minimum_required(VERSION 3.15)

set(PICO_BOARD pico)
set(PICO_PLATFORM rp2040)

include(pico_sdk_import.cmake)

project(321_audio_recorder)

pico_sdk_init()

add_compile_options(-Wall)
# Общие библиотеки лежат рядом с проектом, в корне репозитория.
add_subdirectory(../libs ${CMAKE_BINARY_DIR}/libs)

# 0 — полностью убрать измерения из собранной прошивки.
target_compile_definitions(profiling PUBLIC PROFILING_ENABLED=1)
# Hardware is contained in platform; the SDK remains PRIVATE.
add_library(platform STATIC
    platform/platform.c
)
target_include_directories(platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(platform PRIVATE pico_stdlib
    hardware_adc
    hardware_dma
)

# The logic library intentionally has NO Pico SDK dependency.
add_library(logic STATIC
    logic/api/api.c
    logic/api/api-callbacks.c
    logic/led-task/led-task.c
    logic/button-task/button-task.c
    logic/pi-task/pi-task.c
    logic/telemetry-task/telemetry-task.c
    logic/block-task/block-task.c
    logic/stream-task/stream-task.c
)
target_include_directories(logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/logic
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(logic
    PUBLIC stdio_text_protocol
    PRIVATE platform systime profiling device logging
)

add_executable(${PROJECT_NAME}
    main.c
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    pico_stdlib
    platform
    logic
    profiling
    logging
)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)



target_compile_definitions(${PROJECT_NAME} PRIVATE
    DEVICE_BOARD="${PICO_BOARD}"
)


pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_add_extra_outputs(${PROJECT_NAME})
