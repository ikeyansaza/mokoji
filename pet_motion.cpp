#include "pet_motion.h"

namespace pet_motion {

namespace {

// ハート（7x7）。上の 2 つの山の間がくぼみ、下が尖る。
const char* const kHeart[HEART_H] = {
    ".##.##.",
    "#######",
    "#######",
    "#######",
    ".#####.",
    "..###..",
    "...#...",
};

}  // namespace

bool isStroke(int t) {
    return t == STROKE_TICK;
}

int bounce(int t) {
    return (t >= STROKE_TICK && t < STROKE_TICK + BOUNCE_LEN) ? -BOUNCE_PX : 0;
}

bool heartShown(int t) {
    return t >= STROKE_TICK;
}

int heartX(int walk_x) {
    return walk_x + HEART_X0;
}

int heartY(int t) {
    return HEART_Y0 - (t - STROKE_TICK) / HEART_RISE_FRAMES;
}

bool heartPixel(int dx, int dy) {
    if (dx < 0 || dx >= HEART_W || dy < 0 || dy >= HEART_H) return false;
    return kHeart[dy][dx] == '#';
}

}  // namespace pet_motion
