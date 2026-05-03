#!/usr/bin/env python3
"""SVG ピクセルアート（rect 並べ式）をモノクロ ASCII に変換する。

参考用 SVG（hitsuji.svg 等）から、OLED 表示向けの bitmap を起こすための
変換ツール。黒(#000000)以外の色は ON、黒は OFF として扱う：
- OLED は白 = 灯 = 表示、黒 = 消灯 = 背景なので、シルエットを灯すと見栄えが良い。
- 元の SVG の「黒い輪郭」は OFF になり、ネガティブスペースとして残る。

使い方：
    python3 svg_to_ascii.py hitsuji.svg
"""
import sys
import re


def parse_svg(path):
    with open(path) as f:
        content = f.read()
    rect_re = re.compile(
        r'<rect\s+x="(\d+)"\s+y="(\d+)"\s+width="(\d+)"\s+height="(\d+)"\s+fill="#([0-9A-Fa-f]+)"',
    )
    cells = []
    cell_w = None
    for m in rect_re.finditer(content):
        x, y, w, h, color = m.group(1, 2, 3, 4, 5)
        x, y, w, h = int(x), int(y), int(w), int(h)
        if cell_w is None:
            cell_w = w
        cells.append((x // cell_w, y // cell_w, color.upper()))
    if not cells:
        return None, 0, 0
    cols = max(c[0] for c in cells) + 1
    rows = max(c[1] for c in cells) + 1
    grid = [["." for _ in range(cols)] for _ in range(rows)]
    for cx, cy, color in cells:
        # 純黒 (#000000) は OFF（背景）、それ以外は ON（灯す）
        grid[cy][cx] = "." if color == "000000" else "#"
    return grid, cols, rows


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    grid, cols, rows = parse_svg(sys.argv[1])
    if grid is None:
        print("rect not found")
        sys.exit(1)
    print(f"=== {sys.argv[1]} ({cols}x{rows}) ===")
    for row in grid:
        print("".join(row))


if __name__ == "__main__":
    main()
