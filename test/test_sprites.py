#!/usr/bin/env python3
"""ワイルド系スプライト（角あり）の形状チェック。

実行: python3 test/test_sprites.py  （リポジトリ直下でも test/ 内でも可）
角は耳の横に巻く形。次の 3 点を検証する。
  - 角が頭・耳から浮いた線にならず、頭と 1 つの塊としてつながっている
  - 左右対称である
  - 角が頭のてっぺんより 1 行を超えて上へ突き出さない（高すぎて角に見えなくなるのを防ぐ）
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

import sprites
import sprites_gen

# 角を持つスプライト → 頭のてっぺんの行。
# MOUFLON_LONGHORN は体が 1 行下にずれていて、頭頂部が row 6（row 5 は根元の毛束）。
HEAD_TOP = {
    "YOUNG_WILD_F": 5,
    "ADULT_MOUFLON_F": 5,
    "ADULT_BIGHORN_F": 5,
    "ADULT_MOUFLON_LONGHORN_F": 6,
    "ADULT_BIGHORN_LONGHORN_F": 5,
}

# 頭のてっぺんから耳・体の上端まで（耳は頭頂 +4 行、体の上端は +5 行）。
# 体全体を対象にすると、手描きの脚・腕の斜め接触まで拾ってしまい角と無関係な失敗になる。
HEAD_REGION_ROWS = 6


def grid(name):
    return sprites_gen.to_ascii(getattr(sprites, name))


def head_region(name):
    return grid(name)[:HEAD_TOP[name] + HEAD_REGION_ROWS]


def components(rows):
    """ON ピクセルを 4 近傍（辺で接する）で連結成分に分ける。成分ごとの座標集合のリストを返す。

    8 近傍だと 1px 幅の斜め線が角で接しているだけでも「つながっている」扱いになり、
    細い輪郭線の角を検出できない。4 近傍なら太さ 2px 以上のストロークだけが連結する。
    """
    on = {(y, x) for y, r in enumerate(rows) for x, c in enumerate(r) if c == "#"}
    comps = []
    while on:
        stack = [on.pop()]
        comp = set(stack)
        while stack:
            y, x = stack.pop()
            for n in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                if n in on:
                    on.remove(n)
                    comp.add(n)
                    stack.append(n)
        comps.append(comp)
    return comps


class HornedSpriteTest(unittest.TestCase):
    def test_no_floating_pixels(self):
        # 角が頭から浮いていると「角」ではなく飾りの点・線に見える
        for name in HEAD_TOP:
            with self.subTest(sprite=name):
                comps = components(head_region(name))
                self.assertEqual(
                    len(comps), 1,
                    f"{name}: {len(comps)} 個に分断（細い線・浮いたピクセルあり）: "
                    + str([sorted(c)[0] for c in comps]),
                )

    def test_symmetric(self):
        # 正面向きなので頭〜耳は左右対称。対称軸は col 11（24 幅の中央 11.5 ではない）ため、
        # 先頭 23 列（col 0..22）で反転比較する。
        for name in HEAD_TOP:
            with self.subTest(sprite=name):
                for y, row in enumerate(head_region(name)):
                    row = row[:23]
                    self.assertEqual(row, row[::-1], f"{name} row {y}: {row}")

    def test_horns_do_not_tower_over_head(self):
        # 耳の横に巻く設計。頭頂より 1 行を超えて上に出ると、頭上の飾りに見えてしまう
        for name, top in HEAD_TOP.items():
            with self.subTest(sprite=name):
                first_on = next(y for y, r in enumerate(grid(name)) if "#" in r)
                self.assertGreaterEqual(
                    first_on, top - 1,
                    f"{name}: 最上段が row {first_on}（頭頂 row {top} より 2 行以上上）",
                )


# 若羊（3 系統）
YOUNG_FORMS = ["YOUNG_MOKO_F", "YOUNG_SUFFOLK_F", "YOUNG_WILD_F"]


class YoungSpriteTest(unittest.TestCase):
    def test_symmetric(self):
        # 正面向きなので左右対称（対称軸は col 11）。最後の行のひづめは全フォーム共通で元から非対称なので除く。
        # YOUNG_MOKO は row 7 が 1px 右へずれて崩れていた
        for name in YOUNG_FORMS:
            with self.subTest(sprite=name):
                rows = grid(name)
                last = max(y for y, r in enumerate(rows) if "#" in r)
                for y in range(last):
                    row = rows[y][:23]
                    self.assertEqual(row, row[::-1], f"{name} row {y}: {rows[y]}")

    def test_families_are_distinct(self):
        # モコ系・サフォーク系・ワイルド系で若羊の絵が同じだと、系統の違いがわからない
        seen = {}
        for name in YOUNG_FORMS:
            key = tuple(grid(name))
            self.assertNotIn(key, seen, f"{name} が {seen.get(key)} と同じ絵")
            seen[key] = name


# 増毛版 → 元になる通常版
FLUFFY_BASE = {
    "ADULT_MERINO_FLUFFY_F": "ADULT_MERINO_F",
    "ADULT_CORRIEDALE_FLUFFY_F": "ADULT_CORRIEDALE_F",
    "ADULT_LINCOLN_FLUFFY_F": "ADULT_LINCOLN_F",
    "ADULT_SUFFOLK_FLUFFY_F": "ADULT_SUFFOLK_F",
    "ADULT_HAMPSHIRE_FLUFFY_F": "ADULT_HAMPSHIRE_F",
}


def on_pixels(rows):
    return {(y, x) for y, r in enumerate(rows) for x, c in enumerate(r) if c == "#"}


class FluffySpriteTest(unittest.TestCase):
    def test_fluffy_keeps_the_base_design(self):
        # 増毛版は「通常版に毛を足したもの」。通常版の絵が欠けたり別の絵になったりしてはいけない
        # （サフォーク・ハンプシャーの増毛版が、別の基本形から作られて通常版と違う絵になっていた）
        for name, base in FLUFFY_BASE.items():
            with self.subTest(sprite=name):
                missing = on_pixels(grid(base)) - on_pixels(grid(name))
                self.assertEqual(missing, set(), f"{name}: {base} の絵が {len(missing)} px 欠けている")

    def test_fluffy_is_fluffier(self):
        # 毛が増えていること
        for name, base in FLUFFY_BASE.items():
            with self.subTest(sprite=name):
                self.assertGreater(len(on_pixels(grid(name))), len(on_pixels(grid(base))))

    def test_wool_is_attached_to_the_body(self):
        # 毛が体から浮いた点・棒になると、毛に見えない。通常版より塊（4 近傍）が増えないこと。
        # 毛が体の別々のパーツ（耳・腕など）をつなげて塊が減るのは、毛が体につながっている証拠なので構わない。
        for name, base in FLUFFY_BASE.items():
            with self.subTest(sprite=name):
                self.assertLessEqual(len(components(grid(name))), len(components(grid(base))),
                                     f"{name}: 毛が体から浮いている")

    def test_wool_is_symmetric(self):
        # 通常版の行が左右対称なら、増毛版の行も左右対称であること（対称軸は col 11）
        for name, base in FLUFFY_BASE.items():
            with self.subTest(sprite=name):
                rows, base_rows = grid(name), grid(base)
                for y, (r, b) in enumerate(zip(rows, base_rows)):
                    if b[:23] == b[:23][::-1]:
                        self.assertEqual(r[:23], r[:23][::-1], f"{name} row {y}: {r}")


if __name__ == "__main__":
    unittest.main()
