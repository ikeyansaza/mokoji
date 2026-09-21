#include "eat_motion.h"

namespace eat_motion {

int direction(int walk_x) {
    return walk_x > 56 ? -1 : 1;
}

bool isBiting(int t) {
    for (int i = 0; i < BITE_COUNT; ++i) {
        if (t >= BITE_START[i] && t < BITE_START[i] + BITE_LEN) return true;
    }
    return false;
}

int bitesDone(int t) {
    int n = 0;
    for (int i = 0; i < BITE_COUNT; ++i) {
        if (t >= BITE_START[i] + BITE_LEN) ++n;
    }
    return n;
}

void lean(int t, int dir, int* dx, int* dy) {
    if (isBiting(t)) {
        *dx = dir * LEAN_PX;
        *dy = LEAN_PX;
    } else {
        *dx = 0;
        *dy = 0;
    }
}

int tuftLeft(int walk_x) {
    // 右に置くとき：羊の右端（2 倍スプライトの内容は x+42 まで、傾きで +3）の外。
    // 左に置くとき：羊の左端（内容は x+8 から、傾きで -3）の外。
    return direction(walk_x) > 0 ? walk_x + 46 : walk_x - 6;
}

bool tuftPixel(int dx, int k, int bites) {
    if (dx < 0 || dx >= TUFT_W || dx % 2 != 0) return false;   // 葉は dx = 0, 2, 4
    if (bites < 0) bites = 0;
    if (bites >= BITE_COUNT) return false;                       // 食べ終えた
    // 食べるたびに、3 枚 → 2 枚 → 1 枚 → なし（各葉の高さ）
    static const int kHeights[BITE_COUNT][3] = {
        { 3, 6, 4 },
        { 2, 4, 3 },
        { 0, 2, 1 },
    };
    return k >= 1 && k <= kHeights[bites][dx / 2];
}

}  // namespace eat_motion
