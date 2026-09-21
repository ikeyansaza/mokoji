#include "eat_motion.h"

namespace eat_motion {

namespace {

// 干し草ロール。外周の輪と、途切れのある内側の輪、中心の点（'#' が点く）。
const char* const kBale[BALE_H] = {
    "....####....",
    "..##....##..",
    ".##......##.",
    ".#..####..#.",
    "#..#....#..#",
    "#..#.#..#..#",
    "#..#..#.#..#",
    "#..#.......#",
    ".#..###...#.",
    ".##......##.",
    "..##....##..",
    "....####....",
};

}  // namespace

int direction(int walk_x) {
    return walk_x > 56 ? -1 : 1;
}

bool isBiting(int t) {
    for (int i = 0; i < BITE_COUNT; ++i) {
        if (t >= BITE_START[i] && t < BITE_START[i] + BITE_LEN) return true;
    }
    return false;
}

int bitesTaken(int t) {
    int n = 0;
    for (int i = 0; i < BITE_COUNT; ++i) {
        if (t >= BITE_START[i]) ++n;
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

int baleLeft(int walk_x) {
    // 右に置くとき：羊の右端（2 倍スプライトの内容は x+42 まで、傾きで +3）の外。
    // 左に置くとき：羊の左端（内容は x+8 から、傾きで -3）の外。
    return direction(walk_x) > 0 ? walk_x + 46 : walk_x - (BALE_W + 2);
}

bool balePixel(int dx, int dy, int bites, int dir) {
    if (dx < 0 || dx >= BALE_W || dy < 0 || dy >= BALE_H) return false;
    if (bites < 0) bites = 0;
    if (bites >= BITE_COUNT) return false;                       // 食べ終えた
    if (kBale[dy][dx] != '#') return false;
    if (bites == 0) return true;

    // かじり跡：羊のいる側の縁の外を中心にした円の内側を欠く。距離は 2 倍して整数で比べる
    // （中心は縁の 1px 外、高さは中央。半径は 1 回目 3.5px、2 回目 6px）。
    const int cx2 = (dir > 0) ? -2 : 2 * BALE_W;                  // 中心の x の 2 倍
    const int cy2 = BALE_H - 1;                                   // 中心の y の 2 倍（= 5.5 の 2 倍）
    const int ddx = 2 * dx - cx2, ddy = 2 * dy - cy2;
    const int dist2 = ddx * ddx + ddy * ddy;                      // 距離の 2 乗の 4 倍
    const int r2 = (bites == 1) ? (7 * 7) : (12 * 12);            // 半径 3.5 / 6.0 の 2 倍の、2 乗
    return dist2 >= r2;
}

}  // namespace eat_motion
