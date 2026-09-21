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

// 太陽（9x9）。中心の円盤と、8 方向の光線。斜めの光線（4 つ）は、フレームによって消えて、きらきらする。
const char* const kSun[SUN_H] = {
    "....#....",
    ".#.....#.",
    "...###...",
    "..#####..",
    "#.#####.#",
    "..#####..",
    "...###...",
    ".#.....#.",
    "....#....",
};

// 雲 A（15x5）と雲 B（11x4）。
constexpr int CLOUD_A_W = 15, CLOUD_A_H = 5;
const char* const kCloudA[CLOUD_A_H] = {
    "....###........",
    "..#######.###..",
    ".#############.",
    "###############",
    ".#############.",
};
constexpr int CLOUD_B_W = 11, CLOUD_B_H = 4;
const char* const kCloudB[CLOUD_B_H] = {
    "...###.....",
    ".#########.",
    "###########",
    ".#########.",
};

bool inBitmap(const char* const* rows, int w, int h, int x, int y, int x0, int y0) {
    const int lx = x - x0, ly = y - y0;
    return lx >= 0 && lx < w && ly >= 0 && ly < h && rows[ly][lx] == '#';
}

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

bool sunPixel(int x, int y, int frame) {
    if (!inBitmap(kSun, SUN_W, SUN_H, x, y, SUN_X, SUN_Y)) return false;
    const int lx = x - SUN_X, ly = y - SUN_Y;
    const bool diagonal_ray = (lx == 1 || lx == 7) && (ly == 1 || ly == 7);
    return !(diagonal_ray && (frame & 1));
}

bool cloudPixel(int x, int y, int tick) {
    if (tick < 0) tick = 0;
    // 雲 A は 2 秒に 1px、雲 B は 3 秒に 1px（tick は約 50ms ごと）。右端を出たら左端から戻る。
    // 初期位置は、左上の太陽から離しておく。
    const int ax = (tick / 40 + 60) % (W + CLOUD_A_W) - CLOUD_A_W;
    const int bx = (tick / 60 + 100) % (W + CLOUD_B_W) - CLOUD_B_W;
    return inBitmap(kCloudA, CLOUD_A_W, CLOUD_A_H, x, y, ax, 3) ||
           inBitmap(kCloudB, CLOUD_B_W, CLOUD_B_H, x, y, bx, 10);
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
