// Host-test stub of pico/rand.h. game.cpp が `#include "pico/rand.h"` するので
// Pico SDK 不在の環境でもコンパイルできるよう、stdlib rand で代替する。
#pragma once
#include <cstdint>
#include <cstdlib>

inline uint32_t get_rand_32() {
    // RAND_MAX は実装依存（多くの環境で 31 ビット）なので 2 回引いて XOR で 32 ビット化
    return uint32_t(std::rand()) ^ (uint32_t(std::rand()) << 16);
}
