#!/usr/bin/env python3
"""oled_preview.html に渡すデータ（oled_preview_data.js）の生成ロジックのテスト。

実行: python3 test/test_preview_data.py  （リポジトリ直下でも test/ 内でも可）
スプライト名から成長段階・系統を割り出す処理と、コミット済みの生成物が
sprites.py の内容と一致していること（sprites.h / oled_preview_data.js）を検証する。
"""

import json
import os
import sys
import unittest

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
sys.path.insert(0, ROOT)

import sprites
import sprites_gen

JS_PREFIX = "window.MOKOJI_DATA = "


def by_id(forms):
    return {f["id"]: f for f in forms}


def parse_js(text):
    """生成した JS から JSON 部分を取り出す（ブラウザ側は window.MOKOJI_DATA として読む）。
    先頭の // コメント行（自動生成の注意書き）は読み飛ばす。"""
    body = "".join(l for l in text.splitlines(keepends=True) if not l.startswith("//"))
    assert body.startswith(JS_PREFIX), body[:40]
    assert body.endswith(";\n"), repr(body[-5:])
    return json.loads(body[len(JS_PREFIX):-2])


class ClassifyTest(unittest.TestCase):
    def test_baby(self):
        f = sprites_gen.classify_form("BABY")
        self.assertEqual((f["stage"], f["family"], f["breed"], f["variant"]), ("BABY", None, None, None))

    def test_young_family(self):
        f = sprites_gen.classify_form("YOUNG_WILD")
        self.assertEqual((f["stage"], f["family"], f["breed"]), ("YOUNG", "WILD", None))

    def test_adult_breed_and_family(self):
        f = sprites_gen.classify_form("ADULT_HAMPSHIRE")
        self.assertEqual((f["stage"], f["family"], f["breed"], f["variant"]), ("ADULT", "SUFFOLK", "HAMPSHIRE", None))

    def test_special_growth(self):
        # 増毛期はモコ・サフォーク系、角長期はワイルド系。系統は元の品種から引く
        fl = sprites_gen.classify_form("ADULT_CORRIEDALE_FLUFFY")
        self.assertEqual((fl["stage"], fl["family"], fl["breed"], fl["variant"]), ("SPECIAL", "MOKO", "CORRIEDALE", "FLUFFY"))
        lh = sprites_gen.classify_form("ADULT_BIGHORN_LONGHORN")
        self.assertEqual((lh["stage"], lh["family"], lh["breed"], lh["variant"]), ("SPECIAL", "WILD", "BIGHORN", "LONGHORN"))

    def test_unknown_name_is_rejected(self):
        # 新しい品種・系統を足したときに、対応表の更新漏れを黙って通さない
        for bad in ("ADULT_UNKNOWN", "YOUNG_UNKNOWN", "ADULT_MERINO_UNKNOWN", "SOMETHING"):
            with self.subTest(name=bad):
                with self.assertRaises(ValueError):
                    sprites_gen.classify_form(bad)


class FormsTest(unittest.TestCase):
    def setUp(self):
        self.forms = sprites_gen.forms()

    def test_every_sprite_belongs_to_exactly_one_form(self):
        names = [f"{f['id']}_{k}" for f in self.forms for k in ("F", "L", "R")]
        self.assertEqual(sorted(names), sorted(sprites_gen.SPRITE_NAMES))

    def test_stage_counts(self):
        counts = {}
        for f in self.forms:
            counts[f["stage"]] = counts.get(f["stage"], 0) + 1
        # ベビー 1 + 若羊 3 系統 + 成体 7 品種 + 特殊成長 7（増毛 5 + 角長 2）
        self.assertEqual(counts, {"BABY": 1, "YOUNG": 3, "ADULT": 7, "SPECIAL": 7})

    def test_forms_are_ordered_by_stage(self):
        order = {"BABY": 0, "YOUNG": 1, "ADULT": 2, "SPECIAL": 3}
        stages = [order[f["stage"]] for f in self.forms]
        self.assertEqual(stages, sorted(stages))

    def test_frames_match_sprites(self):
        for f in self.forms:
            for k in ("F", "L", "R"):
                with self.subTest(sprite=f"{f['id']}_{k}"):
                    rows = f["frames"][k]
                    self.assertEqual(rows, sprites_gen.to_ascii(getattr(sprites, f"{f['id']}_{k}")))
                    self.assertEqual(len(rows), 24)
                    self.assertTrue(all(len(r) == 24 for r in rows))

    def test_labels_are_present(self):
        for f in self.forms:
            with self.subTest(form=f["id"]):
                self.assertTrue(f["label"])


class GeneratedFilesTest(unittest.TestCase):
    def test_payload_round_trips(self):
        data = parse_js(sprites_gen.gen_preview_data())
        self.assertEqual([f["id"] for f in data["forms"]], [f["id"] for f in sprites_gen.forms()])
        # 表示用の見出し（段階・系統）も一緒に渡す
        self.assertEqual([s["id"] for s in data["stages"]], ["BABY", "YOUNG", "ADULT", "SPECIAL"])
        self.assertEqual([s["id"] for s in data["families"]], ["MOKO", "SUFFOLK", "WILD"])

    def test_committed_preview_data_is_up_to_date(self):
        with open(os.path.join(ROOT, "oled_preview_data.js"), encoding="utf-8") as f:
            self.assertEqual(f.read(), sprites_gen.gen_preview_data(),
                             "oled_preview_data.js が古い。python3 sprites_gen.py で再生成する")

    def test_committed_sprites_h_is_up_to_date(self):
        with open(os.path.join(ROOT, "sprites.h"), encoding="utf-8") as f:
            self.assertEqual(f.read(), sprites_gen.gen_header(),
                             "sprites.h が古い。python3 sprites_gen.py で再生成する")


if __name__ == "__main__":
    unittest.main()
