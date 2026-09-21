#include "background.h"
#include <cstdint>

namespace background {

namespace {

constexpr int TUFT_COUNT = 16;   // 8px おきに 1 房

// 決定的な疑似乱数（0〜255）。
uint32_t hash8(uint32_t i) {
    uint32_t x = i * 2654435761u;
    x ^= x >> 15;
    x *= 2246822519u;
    x ^= x >> 13;
    return x & 0xFFu;
}

// 三日月（")" 型）。'#' が点く。
const char* const kMoon[MOON_H] = {
    "..##....",
    "...###..",
    "....###.",
    "....###.",
    ".....###",
    ".....###",
    ".....###",
    "....###.",
    "....###.",
    "...###..",
    "..##....",
};

}  // namespace

const int STARS[STAR_COUNT][2] = {
    {10, 12}, {26, 5}, {40, 14}, {54, 4}, {70, 10},
    {86, 3},  {96, 15}, {33, 9}, {78, 16}, {100, 9},
};

bool grassPixel(int x, int y) {
    if (x < 0 || x >= W || y < GRASS_TOP || y >= H) return false;

    // 一番下の行は点線
    if (y == H - 1 && (x + int(hash8(uint32_t(x)))) % 3 != 0) return true;

    const int k = H - 1 - y;   // 地面からの高さ
    for (int i = 0; i < TUFT_COUNT; ++i) {
        const int x0 = 3 + i * 8 + int(hash8(uint32_t(i)) % 4u);        // 房の根元
        const int ht = 3 + int(hash8(uint32_t(i * 7 + 1)) % 4u);        // 中央の葉の高さ 3〜6
        if (x == x0 && k <= ht) return true;
        // 左右の葉は中央より 2 低く、てっぺんだけ外へ 1px 傾く
        const int sh = (ht - 2 > 2) ? ht - 2 : 2;
        if (k <= sh) {
            const int off = (k == sh) ? 3 : 2;
            if (x == x0 - off || x == x0 + off) return true;
        }
    }
    return false;
}

bool moonPixel(int x, int y) {
    const int lx = x - MOON_X, ly = y - MOON_Y;
    if (lx < 0 || lx >= MOON_W || ly < 0 || ly >= MOON_H) return false;
    return kMoon[ly][lx] == '#';
}

bool starPixel(int x, int y, int frame) {
    for (int i = 0; i < STAR_COUNT; ++i) {
        const int dx = x - STARS[i][0], dy = y - STARS[i][1];
        if (dx == 0 && dy == 0) return true;
        const int manhattan = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        if (manhattan == 1 && (i + frame) % 3 == 0) return true;   // 十字（＋）にまたたく星
    }
    return false;
}

}  // namespace background
