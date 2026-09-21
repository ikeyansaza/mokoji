#!/usr/bin/env python3
"""全フォームの足元（羊が立つ位置）がそろっているかのテスト。

実行: python3 test/test_sprite_baseline.py  （リポジトリ直下でも test/ 内でも可）
ゲームはスプライトを固定位置に 2 倍で描くので、足元の行がフォームごとに違うと、
スプライト 1 行あたり画面 2px、進化のたびに羊が上下にずれて見える。
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

import sprites_gen

# 足元（最後に絵がある行 = ひづめ）の標準
BASELINE = 19

# 標準と違う足元を意図しているフォーム
# MOUFLON_LONGHORN は、角を高く伸ばすために体ごと 1 行下げて作ってある
EXCEPTIONS = {"ADULT_MOUFLON_LONGHORN": 20}


def feet_row(rows):
    return max(y for y, r in enumerate(rows) if "#" in r)


class SpriteBaselineTest(unittest.TestCase):
    def test_all_forms_stand_on_the_same_baseline(self):
        for f in sprites_gen.forms():
            expected = EXCEPTIONS.get(f["id"], BASELINE)
            for k in ("F", "L", "R"):
                with self.subTest(sprite=f"{f['id']}_{k}"):
                    self.assertEqual(
                        feet_row(f["frames"][k]), expected,
                        f"{f['id']}_{k}: 足元が row {feet_row(f['frames'][k])}（標準は row {expected}）。"
                        "進化のたびに羊が上下にずれて見える",
                    )


if __name__ == "__main__":
    unittest.main()
