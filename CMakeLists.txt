cmake_minimum_required(VERSION 3.20)
project(MiniMotoNav C CXX)

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
    src/app/moto_app.cpp
    src/ui/ui_speedo.cpp
    src/ui/ui_nav.cpp
    src/ui/ui_controls.cpp
    src/ui/ui_draw.cpp
    src/ui/fonts/font_speed_58.c
    src/ui/fonts/font_ui_10.c
    src/ui/fonts/font_ui_12.c
    src/ui/fonts/font_ui_16.c
    src/ui/fonts/font_ui_20.c
)

add_executable(MiniMotoNav
    sim/main.cpp
    sim/sim_platform.cpp
    ${SHARED_SOURCES}
)

target_include_directories(MiniMotoNav PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2/include
)

target_compile_definitions(MiniMotoNav PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(MiniMotoNav PRIVATE
    lvgl
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2/lib/x64/SDL2.lib
)

if(MSVC)
    target_compile_options(MiniMotoNav PRIVATE /O2 /Gy /utf-8)
    target_compile_options(lvgl PRIVATE /O2)
    set_property(TARGET MiniMotoNav PROPERTY LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()

add_custom_command(TARGET MiniMotoNav POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2/lib/x64/SDL2.dll"
        "$<TARGET_FILE_DIR:MiniMotoNav>/SDL2.dll"
)

enable_testing()
add_executable(telemetry_tests tests/telemetry_tests.cpp src/app/moto_telemetry.cpp)
target_include_directories(telemetry_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
    target_compile_options(telemetry_tests PRIVATE /O2 /utf-8)
endif()
add_test(NAME telemetry_tests COMMAND telemetry_tests)
