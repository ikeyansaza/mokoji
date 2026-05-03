#!/usr/bin/env python3
"""sprites.py の Python リストから sprites.h (C++ 配列) を生成する。

使い方:
    python3 sprites_gen.py            # sprites.h を上書き生成
    python3 sprites_gen.py preview    # sprites.py のスプライトを ASCII で確認
"""

import sys
import sprites

# スプライトのピクセル幅・高さ。sprites.py の各 _img(...) に渡す行の長さ・本数と一致させる。
W = 24
H = 24
ROW_BYTES = (W + 7) // 8

SPRITE_NAMES = [
    # === 新仕様（16 種、ユーザーが順次オリジナル絵に差し替える） ===
    # Stage 1: BABY
    "BABY_F", "BABY_L", "BABY_R",
    # Stage 2: YOUNG（3 系統）
    "YOUNG_MOKO_F",    "YOUNG_MOKO_L",    "YOUNG_MOKO_R",
    "YOUNG_SUFFOLK_F", "YOUNG_SUFFOLK_L", "YOUNG_SUFFOLK_R",
    "YOUNG_WILD_F",    "YOUNG_WILD_L",    "YOUNG_WILD_R",
    # Stage 3: ADULT（6 品種）
    "ADULT_MERINO_F",     "ADULT_MERINO_L",     "ADULT_MERINO_R",
    "ADULT_CORRIEDALE_F", "ADULT_CORRIEDALE_L", "ADULT_CORRIEDALE_R",
    "ADULT_SUFFOLK_F",    "ADULT_SUFFOLK_L",    "ADULT_SUFFOLK_R",
    "ADULT_HAMPSHIRE_F",  "ADULT_HAMPSHIRE_L",  "ADULT_HAMPSHIRE_R",
    "ADULT_MOUFLON_F",    "ADULT_MOUFLON_L",    "ADULT_MOUFLON_R",
    "ADULT_BIGHORN_F",    "ADULT_BIGHORN_L",    "ADULT_BIGHORN_R",
    # Stage 4: SPECIAL（増毛 4 + 角長 2）
    "ADULT_MERINO_FLUFFY_F",     "ADULT_MERINO_FLUFFY_L",     "ADULT_MERINO_FLUFFY_R",
    "ADULT_CORRIEDALE_FLUFFY_F", "ADULT_CORRIEDALE_FLUFFY_L", "ADULT_CORRIEDALE_FLUFFY_R",
    "ADULT_SUFFOLK_FLUFFY_F",    "ADULT_SUFFOLK_FLUFFY_L",    "ADULT_SUFFOLK_FLUFFY_R",
    "ADULT_HAMPSHIRE_FLUFFY_F",  "ADULT_HAMPSHIRE_FLUFFY_L",  "ADULT_HAMPSHIRE_FLUFFY_R",
    "ADULT_MOUFLON_LONGHORN_F",  "ADULT_MOUFLON_LONGHORN_L",  "ADULT_MOUFLON_LONGHORN_R",
    "ADULT_BIGHORN_LONGHORN_F",  "ADULT_BIGHORN_LONGHORN_L",  "ADULT_BIGHORN_LONGHORN_R",

    # === 旧名エイリアス（game.cpp / display.cpp の段階的移行用） ===
    "LAMB_F",    "LAMB_L",    "LAMB_R",
    "YOUNG_SURA_F", "YOUNG_SURA_L", "YOUNG_SURA_R",
    "YOUNG_RARE_F", "YOUNG_RARE_L", "YOUNG_RARE_R",
    "ADULT_COR_F",  "ADULT_COR_L",  "ADULT_COR_R",
    "ADULT_MER_F",  "ADULT_MER_L",  "ADULT_MER_R",
    "ADULT_SUF_F",  "ADULT_SUF_L",  "ADULT_SUF_R",
    "ADULT_SOU_F",  "ADULT_SOU_L",  "ADULT_SOU_R",
    "ADULT_EAST_F", "ADULT_EAST_L", "ADULT_EAST_R",
]


def to_ascii(sprite):
    """W×H スプライトを ASCII プレビューで表示"""
    lines = []
    for row in sprite:
        v = 0
        for b in range(ROW_BYTES):
            v |= row[b] << ((ROW_BYTES - 1 - b) * 8)
        s = ""
        for i in range(W):
            s += "#" if (v >> (W - 1 - i)) & 1 else "."
        lines.append(s)
    return lines


def gen_header():
    out = []
    out.append("// auto-generated from sprites.py — do not edit by hand")
    out.append("// 再生成: python3 sprites_gen.py")
    out.append("#pragma once")
    out.append("#include <cstdint>")
    out.append("")
    out.append("namespace sprites {")
    out.append("")
    out.append(f"constexpr int W = {W};")
    out.append(f"constexpr int H = {H};")
    out.append(f"constexpr int ROW_BYTES = {ROW_BYTES};")
    out.append("")
    for name in SPRITE_NAMES:
        sprite = getattr(sprites, name)
        out.append(f"constexpr uint8_t {name}[H][ROW_BYTES] = {{")
        for row in sprite:
            hex_bytes = ", ".join(f"0x{b:02X}" for b in row)
            out.append(f"    {{{hex_bytes}}},")
        out.append("};")
        out.append("")
    out.append("}  // namespace sprites")
    return "\n".join(out) + "\n"


def preview():
    """全スプライトを ASCII で確認"""
    for name in SPRITE_NAMES:
        sprite = getattr(sprites, name)
        print(f"=== {name} ===")
        for line in to_ascii(sprite):
            print(line)
        print()


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "preview":
        preview()
    else:
        with open("sprites.h", "w") as f:
            f.write(gen_header())
        print("sprites.h を再生成しました")
