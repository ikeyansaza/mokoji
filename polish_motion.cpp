#include "polish_motion.h"

namespace polish_motion {

namespace {

// 火花（5x5）。中心から 4 方向に散る星。
const char* const kSpark[SPARK_H] = {
    "..#..",
    "#.#.#",
    ".###.",
    "#.#.#",
    "..#..",
};

}  // namespace

int direction(int walk_x) {
    return walk_x > 56 ? -1 : 1;
}

int trunkLeft(int walk_x) {
    // 右に置くとき：羊の右端（2 倍スプライトの内容は x+43 まで）の 2px 外。
    // 左に置くとき：羊の左端（内容は x+4 から）の 2px 外に、幹の右端がくる。
    return direction(walk_x) > 0 ? walk_x + 46 : walk_x - 6;
}

bool isRubbing(int t) {
    return t >= RUB_START && t < RUB_END;
}

bool isPressed(int t) {
    return isRubbing(t) && ((t / 2) & 1) == 0;
}

bool filed(int t) {
    return t >= FILED_TICK;
}

void lean(int t, int dir, int* dx, int* dy) {
    *dy = 0;
    if (!isRubbing(t)) {
        *dx = 0;
        return;
    }
    *dx = dir * (isPressed(t) ? LEAN_PX : LEAN_PX - 2);
}

bool sparkShown(int t) {
    return isPressed(t);
}

void sparkPos(int walk_x, int* x, int* y) {
    // 幹の羊側の面に接して、角の高さに出す。
    *x = direction(walk_x) > 0 ? trunkLeft(walk_x) - SPARK_W : trunkLeft(walk_x) + TRUNK_W;
    *y = SPARK_CENTER_Y - SPARK_H / 2;
}

bool trunkPixel(int dx, int dy) {
    if (dx < 0 || dx >= TRUNK_W || dy < 0 || dy >= TRUNK_H) return false;
    if (dx == 0 || dx == TRUNK_W - 1 || dy == 0) return true;   // 輪郭（上端は閉じる）
    // 樹皮の筋（短い横線を、ずらして並べる）
    if ((dx == 2 || dx == 3) && dy % 8 == 3) return true;
    if (dx == 5 && (dy % 8 == 6 || dy % 8 == 7)) return true;
    return false;
}

bool sparkPixel(int dx, int dy) {
    if (dx < 0 || dx >= SPARK_W || dy < 0 || dy >= SPARK_H) return false;
    return kSpark[dy][dx] == '#';
}

}  // namespace polish_motion
