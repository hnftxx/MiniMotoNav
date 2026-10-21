cmake_minimum_required(VERSION 3.20)
project(GODSPEED C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/lvgl EXCLUDE_FROM_ALL)

# LVGL's CMake exports dllimport for consumers which breaks a static link on MSVC.
set_property(TARGET lvgl PROPERTY INTERFACE_COMPILE_DEFINITIONS "")

set(SHARED_SOURCES
    src/app/moto_telemetry.cpp
    src/app/drag_session.cpp
    src/app/perf_model.cpp
    src/app/moto_command.cpp
    src/app/moto_ble_protocol.cpp
    src/app/display_feed.cpp
    src/app/moto_app.cpp
    src/boot/boot_assets.cpp
    src/boot/boot_player.cpp
    src/ui/ui_speedo.cpp
    src/ui/ui_nav.cpp
    src/ui/ui_drag.cpp
    src/ui/ui_gps_clock.cpp
    src/ui/ui_controls.cpp
    src/ui/ui_draw.cpp
    src/ui/fonts/font_speed_58.c
    src/ui/fonts/font_timer_46.c
    src/ui/fonts/font_count_96.c
    src/ui/fonts/font_ui_10.c
    src/ui/fonts/font_ui_12.c
    src/ui/fonts/font_ui_16.c
    src/ui/fonts/font_ui_20.c
)

add_executable(GODSPEED
    sim/main.cpp
    sim/sim_platform.cpp
    ${SHARED_SOURCES}
)

target_include_directories(GODSPEED PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2/include
)

target_compile_definitions(GODSPEED PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(GODSPEED PRIVATE
    lvgl
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2/lib/x64/SDL2.lib
)

if(MSVC)
    target_compile_options(GODSPEED PRIVATE /O2 /Gy /utf-8)
    target_compile_options(lvgl PRIVATE /O2)
    set_property(TARGET GODSPEED PROPERTY LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()

add_custom_command(TARGET GODSPEED POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2/lib/x64/SDL2.dll"
        "$<TARGET_FILE_DIR:GODSPEED>/SDL2.dll"
)

enable_testing()
add_executable(telemetry_tests tests/telemetry_tests.cpp src/app/moto_telemetry.cpp)
target_include_directories(telemetry_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
    target_compile_options(telemetry_tests PRIVATE /O2 /utf-8)
endif()
add_test(NAME telemetry_tests COMMAND telemetry_tests)

add_executable(drag_tests tests/drag_tests.cpp src/app/drag_session.cpp)
target_include_directories(drag_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
    target_compile_options(drag_tests PRIVATE /O2 /utf-8)
endif()
add_test(NAME drag_tests COMMAND drag_tests)

add_executable(perf_tests tests/perf_tests.cpp src/app/perf_model.cpp)
target_include_directories(perf_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
    target_compile_options(perf_tests PRIVATE /O2 /utf-8)
endif()
add_test(NAME perf_tests COMMAND perf_tests)

add_executable(command_tests tests/command_tests.cpp src/app/moto_command.cpp)
target_include_directories(command_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
    target_compile_options(command_tests PRIVATE /O2 /utf-8)
endif()
add_test(NAME command_tests COMMAND command_tests)

add_executable(
    ble_protocol_tests
    tests/ble_protocol_tests.cpp
    src/app/moto_ble_protocol.cpp
    src/app/display_feed.cpp
)
target_include_directories(ble_protocol_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
    target_compile_options(ble_protocol_tests PRIVATE /O2 /utf-8)
endif()
add_test(NAME ble_protocol_tests COMMAND ble_protocol_tests)

add_executable(
    boot_tests
    tests/boot_tests.cpp
    src/boot/boot_player.cpp
    src/boot/boot_assets.cpp
)
target_include_directories(boot_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
    target_compile_options(boot_tests PRIVATE /O2 /utf-8)
endif()
add_test(NAME boot_tests COMMAND boot_tests)
