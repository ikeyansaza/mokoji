cmake_minimum_required(VERSION 3.13)

# pico-sdk から PICO_SDK_PATH を読み込み（環境変数 or -DPICO_SDK_PATH=... で指定）
include(pico_sdk_import.cmake)

project(mokoji C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(mokoji
    main.cpp
    game.cpp
    jump_game.cpp
    background.cpp
    display.cpp
    sound.cpp
    save.cpp
    ssd1306.cpp
    font.cpp
    kana.cpp
)

target_link_libraries(mokoji
    pico_stdlib
    hardware_i2c
    hardware_pwm
    hardware_flash
    hardware_sync
    pico_rand
)

# シリアルは USB CDC を使う（ボード上のデバッグ・ログ用）
pico_enable_stdio_usb(mokoji 1)
pico_enable_stdio_uart(mokoji 0)

# .uf2 / .hex / .bin / .map / .dis を出力
pico_add_extra_outputs(mokoji)

# 実機での進化検証用：1 game-hour = 3 秒に短縮するモード
option(DEBUG_FAST "Compress game time for hardware verification (3 sec / game-hour)" OFF)
if(DEBUG_FAST)
    target_compile_definitions(mokoji PRIVATE DEBUG_FAST=1)
    message(STATUS "DEBUG_FAST=ON: 1 game-hour = 3 sec real, 1 day = 72 sec, evolution at ~3.6 min / ~8.4 min")
endif()
