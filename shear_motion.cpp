#include "shear_motion.h"

namespace shear_motion {

namespace {

// はさみ（7x9）。上の刃が X 字に交わり、下に持ち手の輪が 2 つ。刃が開いた絵と、閉じた絵。
const char* const kScissorsOpen[SCISSORS_H] = {
    "#.....#",
    ".#...#.",
    "..#.#..",
    "...#...",
    "..#.#..",
    ".##.##.",
    "#..#..#",
    "#..#..#",
    ".##.##.",
};
const char* const kScissorsClosed[SCISSORS_H] = {
    "...#...",
    "...#...",
    "...#...",
    "...#...",
    "..#.#..",
    ".##.##.",
    "#..#..#",
    "#..#..#",
    ".##.##.",
};

// 毛の束（5x4）。ふわっとした丸い塊。
const char* const kTuft[TUFT_H] = {
    ".#.#.",
    "#####",
    "#####",
    ".###.",
};

int passOf(int t) { return t >= PASS_START[1] ? 1 : 0; }

}  // namespace

bool clipperShown(int t) {
    return t >= PASS_START[0] && t < CUT_TICK;
}

int clipperX(int walk_x, int t) {
    const int pass = passOf(t);
    int k = t - PASS_START[pass];
    if (k < 0) k = 0;
    if (k > PASS_LEN - 1) k = PASS_LEN - 1;
    const int off = k * (PASS_X1 - PASS_X0) / (PASS_LEN - 1);
    return walk_x + (pass == 0 ? PASS_X0 + off : PASS_X1 - off);   // 1 回目は右へ、2 回目は左へ
}

int clipperY(int t) {
    return PASS_Y[passOf(t)];
}

bool clipperOpen(int t) {
    return ((t / 2) & 1) == 0;
}

bool woolCut(int t) {
    return t >= CUT_TICK;
}

bool tuft(int i, int walk_x, int t, int* x, int* y) {
    if (i < 0 || i >= TUFT_COUNT || t < TUFT_SPAWN[i]) return false;
    // はさみの刃の下から落ち始める。地面に着いたら、そこに積もる。
    *x = clipperX(walk_x, TUFT_SPAWN[i]) + (SCISSORS_W - TUFT_W) / 2;
    const int rest = GROUND_Y - TUFT_H + 1;
    const int fallen = clipperY(TUFT_SPAWN[i]) + SCISSORS_H + (t - TUFT_SPAWN[i]) * TUFT_FALL_PX;
    *y = fallen < rest ? fallen : rest;
    return true;
}

bool scissorsPixel(bool open, int dx, int dy) {
    if (dx < 0 || dx >= SCISSORS_W || dy < 0 || dy >= SCISSORS_H) return false;
    return (open ? kScissorsOpen : kScissorsClosed)[dy][dx] == '#';
}

bool tuftPixel(int dx, int dy) {
    if (dx < 0 || dx >= TUFT_W || dy < 0 || dy >= TUFT_H) return false;
    return kTuft[dy][dx] == '#';
}

}  // namespace shear_motion
