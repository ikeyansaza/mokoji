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


CANDIDATES_DIR = os.path.join(ROOT, "docs", "sprite-candidates")


class CandidatesTest(unittest.TestCase):
    """docs/sprite-candidates/*/candidates.txt の候補を oled_preview.html に渡すデータのテスト。"""

    def setUp(self):
        self.cands = sprites_gen.candidates()

    def test_kind_of_status(self):
        # 採用 / 没（未採用・外した・旧デザイン）/ その他（B 案のベースなど）に分ける
        kind = sprites_gen.candidate_kind
        self.assertEqual(kind("採用: YOUNG_MOKO"), "adopted")
        self.assertEqual(kind("採用: MOUFLON_LONGHORN（1 行下げて使用）"), "adopted")
        self.assertEqual(kind("未採用"), "rejected")
        self.assertEqual(kind("初期案・未採用（見立て: 小さすぎる）"), "rejected")
        self.assertEqual(kind("外した（B01 と大差なし）"), "rejected")
        self.assertEqual(kind("旧デザイン（M0 として、モコ系が引き継いだ）"), "rejected")
        self.assertEqual(kind("初期案・これをベースに B01〜B10 へ展開"), "other")

    def test_unknown_status_is_rejected(self):
        # 新しい状態の言い回しを足したときに、分類の更新漏れを黙って通さない
        with self.assertRaises(ValueError):
            sprites_gen.candidate_kind("よくわからない")

    def test_every_sprite_in_candidates_files_is_loaded(self):
        for topic in sorted({c["topic"] for c in self.cands}):
            with open(os.path.join(CANDIDATES_DIR, topic, "candidates.txt"), encoding="utf-8") as f:
                in_file = len([l for l in f if l.startswith(tuple("ABCDEFGHIJKLMNOPQRSTUVWXYZ")) and " = (" in l])
            with self.subTest(topic=topic):
                self.assertEqual(len([c for c in self.cands if c["topic"] == topic]), in_file)

    def test_all_topic_folders_are_covered(self):
        # docs に新しいトピックのフォルダを足したら、sprites_gen.py の TOPICS にも足す
        folders = sorted(d for d in os.listdir(CANDIDATES_DIR) if os.path.isdir(os.path.join(CANDIDATES_DIR, d)))
        self.assertEqual(sorted({c["topic"] for c in self.cands}), folders)

    def test_fields(self):
        ids = [c["id"] for c in self.cands]
        self.assertEqual(len(ids), len(set(ids)), "名前が重複している")
        for c in self.cands:
            with self.subTest(candidate=c["id"]):
                for key in ("topic", "topicLabel", "id", "short", "feature", "status", "kind", "rows"):
                    self.assertTrue(c[key], f"{key} が空")
                self.assertIn(c["kind"], ("adopted", "rejected", "other"))
                self.assertEqual(len(c["rows"]), 24)
                self.assertTrue(all(len(r) == 24 and set(r) <= {".", "#"} for r in c["rows"]))

    def test_adopted_candidates_match_the_game(self):
        # 「採用: <フォーム名>」と書いた案は、実際のゲームのスプライトと同じ絵であること（記録と実物の食い違いを防ぐ）。
        # 角の案は MOUFLON の体に描いた見本で、YOUNG_WILD などの体とは違うので対象外。
        forms = sprites_gen.forms()
        by_id = {f["id"]: f for f in forms}
        checked = 0
        for c in self.cands:
            if c["kind"] != "adopted" or c["topic"] == "horn":
                continue
            target = c["status"].split(":", 1)[1].strip().split("（")[0]
            if target in by_id:
                checked += 1
                with self.subTest(candidate=c["id"], form=target):
                    self.assertEqual(c["rows"], by_id[target]["frames"]["F"])
        self.assertGreater(checked, 5, "突き合わせた採用案が少なすぎる")

    def test_payload_includes_candidates(self):
        data = parse_js(sprites_gen.gen_preview_data())
        self.assertEqual([c["id"] for c in data["candidates"]], [c["id"] for c in self.cands])
        self.assertEqual([t["id"] for t in data["topics"]], sorted({c["topic"] for c in self.cands}, key=lambda t: [x[0] for x in sprites_gen.TOPICS].index(t)))


if __name__ == "__main__":
    unittest.main()
