#!/usr/bin/env python3
"""美咲フォント (BDF) から 8x8 ひらがなグリフを抜き出し、kana_font.h を生成する。

使い方:
    python3 kana_font_gen.py path/to/misaki_gothic.bdf      # kana_font.h を上書き生成
    python3 kana_font_gen.py path/to/misaki_gothic.bdf preview   # ASCII で確認

BDF の入手先: https://littlelimit.net/misaki.htm （misaki_bdf_*.zip の misaki_gothic.bdf）
ライセンス: third_party/misaki/misaki.txt（商用可・再配布自由・無保証）

ひらがなの並びは kana.cpp の「/* 番号 文字 */」コメントを唯一の元データとして読む。
kana.cpp の index 体系を変えたら、このスクリプトを再実行すれば kana_font.h が追従する。
"""

import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
CELL = 8  # 8x8 セル


def read_kana_order():
    """kana.cpp のコメントから index -> ひらがな1文字 の表を作る。"""
    text = (HERE / "kana.cpp").read_text(encoding="utf-8")
    table = {}
    for m in re.finditer(r"/\*\s*(\d+)\s+(\S)\s*\*/\s*\{\"", text):
        table[int(m.group(1))] = m.group(2)
    count = len(table)
    if sorted(table) != list(range(count)):
        raise SystemExit("kana.cpp のコメントから index が連番で取れませんでした")
    return [table[i] for i in range(count)]


def parse_bdf(path):
    """BDF を {unicode: (w, h, xoff, yoff, [row_byte...])} に読む。cell 下端/上端も返す。"""
    glyphs = {}
    fbb = None
    cur = None
    in_bitmap = False
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        if line.startswith("FONTBOUNDINGBOX"):
            fbb = tuple(int(v) for v in line.split()[1:5])   # w h xoff yoff
        elif line.startswith("STARTCHAR"):
            cur = {"rows": []}
            in_bitmap = False
        elif cur is not None and line.startswith("ENCODING"):
            cur["enc"] = int(line.split()[1])
        elif cur is not None and line.startswith("BBX"):
            cur["bbx"] = tuple(int(v) for v in line.split()[1:5])
        elif line == "BITMAP":
            in_bitmap = True
        elif line == "ENDCHAR":
            if cur is not None and "enc" in cur and "bbx" in cur:
                glyphs[cur["enc"]] = (cur["bbx"], cur["rows"])
            cur = None
            in_bitmap = False
        elif in_bitmap and cur is not None:
            cur["rows"].append(int(line, 16))
    if fbb is None:
        raise SystemExit("FONTBOUNDINGBOX が見つかりません")
    return glyphs, fbb


def to_cell(bbx, rows, fbb):
    """BDF の切り詰めビットマップを 8x8 セル（1 行 1 バイト、bit7 = 左端）に置き直す。"""
    w, h, xoff, yoff = bbx
    _, fh, _, fyoff = fbb
    cell_top_y = fyoff + fh - 1              # セル最上段の y 座標（BDF は上が +）
    first_row = cell_top_y - (yoff + h - 1)  # グリフ最上段がセルの何行目か
    out = [0] * CELL
    for i, byte in enumerate(rows):
        r = first_row + i
        if 0 <= r < CELL:
            out[r] = (byte >> xoff) & 0xFF
    return out


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    order = read_kana_order()
    glyphs, fbb = parse_bdf(sys.argv[1])

    cells = []
    for ch in order:
        enc = ord(ch)
        if enc not in glyphs:
            raise SystemExit(f"BDF に {ch} (U+{enc:04X}) がありません")
        bbx, rows = glyphs[enc]
        cells.append(to_cell(bbx, rows, fbb))

    if len(sys.argv) > 2 and sys.argv[2] == "preview":
        for ch, cell in zip(order, cells):
            print(ch)
            for b in cell:
                print("  " + "".join("#" if b & (0x80 >> c) else "." for c in range(CELL)))
        return

    lines = [
        "// 自動生成ファイル。手編集しない。生成元: kana_font_gen.py",
        "// フォント: 美咲ゴシック 8x8 (Copyright (C) 2002-2021 Num Kadoma)",
        "//   ライセンス: third_party/misaki/misaki.txt",
        "// 並び: kana.cpp の index 順（1 グリフ 8 バイト、1 行 1 バイト、bit7 = 左端）",
        "#pragma once",
        "#include <cstdint>",
        "",
        f"constexpr int KANA_FONT_COUNT = {len(order)};",
        "",
        f"constexpr uint8_t kKanaFont[KANA_FONT_COUNT][{CELL}] = {{",
    ]
    for i, (ch, cell) in enumerate(zip(order, cells)):
        body = ", ".join(f"0x{b:02X}" for b in cell)
        lines.append(f"    /* {i:2d} {ch} */ {{ {body} }},")
    lines.append("};")
    (HERE / "kana_font.h").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"kana_font.h を生成しました（{len(order)} 字）")


if __name__ == "__main__":
    main()
