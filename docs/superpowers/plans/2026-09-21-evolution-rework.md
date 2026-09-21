# 進化ルールの見直しと、若羊・成体のスプライト更新 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 進化を空腹・幸福の値と乱数で決め、毛・角の世話を成体だけにし、増毛のキラキラを止め、若羊・成体の絵と成体の足を更新する。

**Architecture:** ゲームのルールは `Game` の中で完結させ、進化の重みは乱数と切り離した純粋な static 関数（`youngChoices` / `adultChoices` / `pickWeighted`）にして、テストで直接確かめる。セーブ形式（`GameSaveData` の構造と `MAGIC`）は変えない。スプライトは `sprites.py` を編集して `sprites_gen.py` で `sprites.h` / `oled_preview_data.js` を再生成する。

**Tech Stack:** C++17（Pico SDK 非依存のロジック + 自前 assert のホストテスト）、Python 3（スプライト生成とその unittest）、CMake / Pico SDK（ファームのビルド）。

**Spec:** `docs/superpowers/specs/2026-09-21-evolution-rework-design.md`

## Global Constraints

- コミットメッセージは日本語。末尾に `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>` を付ける。
- テストを先に書いて、失敗を確認してから実装する（TDD）。複雑なロジックにはコメントを付ける。
- セーブ形式は変えない: `GameSaveData` の構造と `MAGIC`（`0x4D4F4B35`）はそのまま。
- `sprites.h` と `oled_preview_data.js` は手編集しない。`python3 sprites_gen.py` で再生成する。
- 足元の行: ベビー・若羊 = row 19、成体（増毛版を含む）= row 20、`ADULT_MOUFLON_LONGHORN` = row 21。
- 若羊のメニューはベビーと同じ 5 項目（ごはん・なでる・ゲーム・プロフ・もどる）。成体は 6 項目（毛刈り・角研ぎが入る）。
- 進化の重みの係数: 基本 50、モコ系は `hunger × 40%`、サフォーク系は `happy × 40%`、ワイルド系は `(200 − hunger − happy) × 20%`、HAMPSHIRE は `happy × 50%`、BIGHORN は `hunger × 50%`。
- 作業は worktree `.claude/worktrees/evolution-rework`（ブランチ `improve/evolution-rework`）の中で行う。ルートには `cd` しない。
- 全確認コマンド（各 Task の最後に流す）:
  `python3 test/test_sprites.py && python3 test/test_sprite_baseline.py && python3 test/test_preview_data.py && (cd test && make test)`

## File Structure

| ファイル | 役割 |
|---|---|
| `sprites.py` | スプライトの絵（若羊・成体の差し替えと足、増毛の毛） |
| `sprites.h` / `oled_preview_data.js` | `sprites_gen.py` が生成（手編集しない） |
| `docs/superpowers/plans/2026-09-21-evolution-rework-sprites.txt` | 貼ってもらった新しい絵 6 種（Task 2 が読む） |
| `docs/sprite-candidates/{young,hampshire,fluffy}/` | 採用案の記録。採用案が実際の絵と一致するテストがある |
| `game.h` / `game.cpp` | 進化の重み、毛・角の伸び、メニュー、`tend_*` の削除 |
| `display.cpp` / `display.h` | `drawWool`（キラキラ）の削除 |
| `save.h` | `tend_*` の注釈のみ（構造は不変） |
| `test/test_game.cpp` | 進化・メニュー・毛・セーブのテスト |
| `test/test_sprites.py` / `test/test_sprite_baseline.py` | 若羊・足元・増毛のテスト |
| `CLAUDE.md` | 進化システムの説明 |

---

### Task 1: 足元の基準をテストで先に更新する（失敗を確認）

**Files:**
- Modify: `test/test_sprite_baseline.py`（全体を置き換える）

**Interfaces:**
- Consumes: `sprites_gen.forms()`（各フォームの `id` / `stage` / `frames`。`stage` は `BABY` / `YOUNG` / `ADULT` / `SPECIAL`）
- Produces: なし（Task 3 の完了で通る）

- [ ] **Step 1: テストを書き換える**

`test/test_sprite_baseline.py` を、次の内容にする。

```python
#!/usr/bin/env python3
"""全フォームの足元（羊が立つ位置）がそろっているかのテスト。

実行: python3 test/test_sprite_baseline.py  （リポジトリ直下でも test/ 内でも可）
ゲームはスプライトを固定位置に 2 倍で描くので、足元の行がフォームごとに違うと、
スプライト 1 行あたり画面 2px、進化のたびに羊が上下にずれて見える。
ベビー・若羊は row 19、成体（増毛版・角長版を含む）は、足が 2 行（棒 + 2 本）なので row 20。
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

import sprites_gen

# 成長段階ごとの足元（最後に絵がある行 = 足の先）
BASELINE = {"BABY": 19, "YOUNG": 19, "ADULT": 20, "SPECIAL": 20}

# 標準と違う足元を意図しているフォーム
# MOUFLON_LONGHORN は、角を高く伸ばすために体ごと 1 行下げて作ってあるので、足元も 1 行低い
EXCEPTIONS = {"ADULT_MOUFLON_LONGHORN": 21}


def feet_row(rows):
    return max(y for y, r in enumerate(rows) if "#" in r)


class SpriteBaselineTest(unittest.TestCase):
    def test_all_forms_stand_on_the_expected_baseline(self):
        for f in sprites_gen.forms():
            expected = EXCEPTIONS.get(f["id"], BASELINE[f["stage"]])
            for k in ("F", "L", "R"):
                with self.subTest(sprite=f"{f['id']}_{k}"):
                    self.assertEqual(
                        feet_row(f["frames"][k]), expected,
                        f"{f['id']}_{k}: 足元が row {feet_row(f['frames'][k])}（期待は row {expected}）。"
                        "進化のたびに羊が上下にずれて見える",
                    )


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 失敗を確認する**

Run: `python3 test/test_sprite_baseline.py 2>&1 | grep -E "^(FAIL|OK|Ran)" | sort | uniq -c | head`
Expected: 成体・増毛版・角長版のフォーム（`ADULT_*`）の F/L/R が、`足元が row 19（期待は row 20）` で失敗する。ベビー・若羊は失敗しない。

---

### Task 2: 若羊 3 種・成体 3 種の絵を差し替える

**Files:**
- Modify: `sprites.py`（`YOUNG_MOKO` / `YOUNG_SUFFOLK` / `YOUNG_WILD` / `SUFFOLK` / `MOUFLON` / `BIGHORN` の絵）
- Read: `docs/superpowers/plans/2026-09-21-evolution-rework-sprites.txt`

**Interfaces:**
- Consumes: 上のデータファイル（`YOUNG_MOKO` / `YOUNG_SUFFOLK` / `YOUNG_WILD` / `ADULT_SUFFOLK` / `ADULT_MOUFLON` / `ADULT_BIGHORN` の各 24 行）
- Produces: `sprites.py` の各タプルが新しい絵になる。`SUFFOLK` / `MOUFLON` / `BIGHORN` の row 19・20 が新しい足になる（Task 3 が他の成体をそろえる基準）

- [ ] **Step 1: 差し替えスクリプトを書いて実行する**

`/private/tmp/claude-501/-Users-naofumiikeda-repositories-mokoji/5ec93858-ae33-4d1c-88e1-013474ae94c6/scratchpad/apply_new_sprites.py`（スクラッチ領域）に、次を書く。

```python
"""sprites.py の若羊 3 種・成体 3 種を、計画に添えたデータファイルの絵に差し替える。"""
import re

DATA = "docs/superpowers/plans/2026-09-21-evolution-rework-sprites.txt"
# データファイルの名前 → sprites.py のタプル名
TARGETS = {
    "YOUNG_MOKO": "YOUNG_MOKO", "YOUNG_SUFFOLK": "YOUNG_SUFFOLK", "YOUNG_WILD": "YOUNG_WILD",
    "ADULT_SUFFOLK": "SUFFOLK", "ADULT_MOUFLON": "MOUFLON", "ADULT_BIGHORN": "BIGHORN",
}

text = open(DATA, encoding="utf-8").read()
new = {m.group(1): re.findall(r'"([.#]{24})"', m.group(2))
       for m in re.finditer(r"^([A-Z_]+) = \(\n(.*?)^\)", text, re.S | re.M)}
assert set(new) == set(TARGETS), set(new) ^ set(TARGETS)

lines = open("sprites.py", encoding="utf-8").read().split("\n")
for src, dst in TARGETS.items():
    rows = new[src]
    assert len(rows) == 24, (src, len(rows))
    start = next(i for i, l in enumerate(lines) if l == f"{dst} = (")
    end = next(i for i in range(start, len(lines)) if lines[i] == ")")
    assert end - start - 1 == 24, (dst, end - start - 1)
    lines[start + 1:end] = [f'    "{r}",' for r in rows]      # 行ごとのコメントは、絵が変わったので外す

# 見出しコメント（YOUNG_MOKO は説明が古くなるので書き直す）
hi = next(i for i, l in enumerate(lines) if l.startswith("# モコ系若羊"))
lines[hi] = "# モコ系若羊：頭の毛玉があり、丸みのある輪郭の体と細い脚（あなたが描いた案）"
open("sprites.py", "w", encoding="utf-8").write("\n".join(lines))
print("差し替え:", ", ".join(TARGETS.values()))
```

Run: `python3 /private/tmp/claude-501/-Users-naofumiikeda-repositories-mokoji/5ec93858-ae33-4d1c-88e1-013474ae94c6/scratchpad/apply_new_sprites.py`
Expected: `差し替え: YOUNG_MOKO, YOUNG_SUFFOLK, YOUNG_WILD, SUFFOLK, MOUFLON, BIGHORN`

- [ ] **Step 2: 再生成して、絵のテストを流す**

Run: `python3 sprites_gen.py && python3 test/test_sprites.py 2>&1 | tail -3`
Expected: `OK`（若羊の左右対称・3 系統が別の絵・ワイルド系がムフロンと別・角の形が通る。ひづめの上の棒が非対称なのは、足元の棒の例外で許可されている）

- [ ] **Step 3: 足元のテストの状態を確認する**

Run: `python3 test/test_sprite_baseline.py 2>&1 | grep -E "^(FAIL|OK|Ran)" | sed 's/(sprite=.*//' | sort | uniq -c`
Expected: `SUFFOLK` / `MOUFLON` / `BIGHORN` の F/L/R は通り、他の成体（メリノ・コリデール・リンカーン・ハンプシャー・ビッグホーン角長・増毛版の一部）と、ムフロン角長が、まだ失敗する（Task 3 で直る）。

---

### Task 3: 他の成体の足をそろえる

**Files:**
- Modify: `sprites.py`（`MERINO` / `CORRIEDALE` / `LINCOLN` / `HAMPSHIRE` / `MOUFLON_LONGHORN` / `BIGHORN_LONGHORN` の足の行）

**Interfaces:**
- Consumes: Task 2 で決まった足の形（ムフロン式 = row +1 が 8px 棒 `........########........`、row +2 が `.........#....#.........`。ビッグホーン式 = `........#######.........` と `.........#...#..........`）
- Produces: 全成体の足元が row 20（ムフロン角長は row 21）。増毛版は通常版から作られるので、自動でそろう

- [ ] **Step 1: 足の差し替えスクリプトを書いて実行する**

スクラッチ領域の `apply_feet.py` に、次を書く。

```python
"""他の成体の足を、貼ってもらった成体（ムフロン・ビッグホーン）と同じ形にそろえる。
足は「棒の行の下に、棒（8px か 7px）の行と、2 本の足の行」を置く。"""
import re

MOUFLON_FEET = ("........########........", ".........#....#.........")     # 幅 11px の棒の成体用
BIGHORN_FEET = ("........#######.........", ".........#...#..........")     # 幅 9px の棒の成体用
OLD_HOOF = ".........##..##........."

# (タプル名, 棒の行, 足の形, 棒の幅)。MOUFLON_LONGHORN は体が 1 行低いので、棒が row 19
PLAN = [
    ("MERINO", 18, MOUFLON_FEET, 11), ("CORRIEDALE", 18, MOUFLON_FEET, 11),
    ("LINCOLN", 18, MOUFLON_FEET, 11), ("HAMPSHIRE", 18, MOUFLON_FEET, 11),
    ("MOUFLON_LONGHORN", 19, MOUFLON_FEET, 11), ("BIGHORN_LONGHORN", 18, BIGHORN_FEET, 9),
]

lines = open("sprites.py", encoding="utf-8").read().split("\n")
row_re = re.compile(r'^(\s*)"([.#]{24})",(.*)$')
for name, bar, feet, width in PLAN:
    start = next(i for i, l in enumerate(lines) if l == f"{name} = (")
    rows = [row_re.match(lines[start + 1 + r]).group(2) for r in range(24)]
    assert rows[bar].count("#") == width, (name, rows[bar])       # 棒の幅が想定どおりか
    assert rows[bar + 1] == OLD_HOOF, (name, rows[bar + 1])        # 今は、棒の下にひづめの行
    assert set(rows[bar + 2]) == {"."}, (name, rows[bar + 2])      # その下は空
    for offset, (row, note) in enumerate(zip(feet, ("# 足（棒）", "# 足"))):
        i = start + 1 + bar + 1 + offset
        indent = row_re.match(lines[i]).group(1)
        lines[i] = f'{indent}"{row}",   {note}'
open("sprites.py", "w", encoding="utf-8").write("\n".join(lines))
print("足を差し替え:", ", ".join(p[0] for p in PLAN))
```

Run: `python3 /private/tmp/claude-501/-Users-naofumiikeda-repositories-mokoji/5ec93858-ae33-4d1c-88e1-013474ae94c6/scratchpad/apply_feet.py`
Expected: `足を差し替え: MERINO, CORRIEDALE, LINCOLN, HAMPSHIRE, MOUFLON_LONGHORN, BIGHORN_LONGHORN`（`HAMPSHIRE` は `HAMPSHIRE = (` として定義されている必要がある。見つからなければ `sprites.py` の `HAMPSHIRE` の定義を確認する）

- [ ] **Step 2: 再生成して、絵の全テストを流す**

Run: `python3 sprites_gen.py && python3 test/test_sprites.py 2>&1 | tail -2 && python3 test/test_sprite_baseline.py 2>&1 | tail -2`
Expected: どちらも `OK`（成体はすべて row 20、ムフロン角長は row 21）

- [ ] **Step 3: 見た目を確認する**

Run: `python3 - <<'EOF'
import sprites, sprites_gen
names = ["ADULT_MERINO", "ADULT_SUFFOLK", "ADULT_HAMPSHIRE", "ADULT_MOUFLON", "ADULT_BIGHORN", "ADULT_MOUFLON_LONGHORN", "ADULT_BIGHORN_LONGHORN"]
fr = {n: sprites_gen.to_ascii(getattr(sprites, n + "_F")) for n in names}
print("  ".join(n.replace("ADULT_", "").ljust(24)[:24] for n in names))
for y in range(14, 23):
    print("  ".join(fr[n][y] for n in names))
EOF`
Expected: どの成体も、棒の下に 8px か 7px の棒、その下に 2 本の足が付いている。

---

### Task 4: docs の記録を実際の絵に合わせ、絵のコミットを作る

**Files:**
- Modify: `docs/sprite-candidates/young/candidates.txt`、`docs/sprite-candidates/young/README.md`、`docs/sprite-candidates/hampshire/candidates.txt`、`docs/sprite-candidates/fluffy/candidates.txt`

**Interfaces:**
- Consumes: `sprites_gen.forms()`（実際のフォームの F フレーム）、Task 2・3 の絵
- Produces: 「採用」と書いた候補が、実際のスプライトと一致する（`test_preview_data.py` の `test_adopted_candidates_match_the_game` が通る）

- [ ] **Step 1: 失敗を確認する**

Run: `python3 test/test_preview_data.py 2>&1 | grep -E "^(FAIL|OK|Ran)" | sed 's/ (.*candidate=/ candidate=/' | head -20`
Expected: `test_adopted_candidates_match_the_game` が、`YOUNG_MOKO_M0` / `YOUNG_SUFFOLK_D2` / `YOUNG_WILD_WD` / `HAMPSHIRE_OUTLINE2` / `FLUFF_*_HALO` で失敗する。`test_committed_preview_data_is_up_to_date` も、再生成前なら失敗する（Step 2 で直る）。

- [ ] **Step 2: docs の同期スクリプトを書いて実行する**

スクラッチ領域の `sync_docs.py` に、次を書く。

```python
"""docs/sprite-candidates の採用案を、実際のスプライトに合わせる。
- 足だけが変わった案（ハンプシャー・増毛）は、絵をその場で更新する。
- 絵そのものが変わった若羊 3 種は、これまでの採用案を「旧デザイン」にし、新しい絵を新しい案として足す。"""
import re
import sys

sys.path.insert(0, ".")
import sprites_gen

forms = {f["id"]: f for f in sprites_gen.forms()}


def read(path):
    return open(path, encoding="utf-8").read()


def write(path, text):
    open(path, "w", encoding="utf-8").write(text)


def set_rows(text, name, rows):
    m = re.search(rf"^{name} = \(\n(.*?)^\)", text, re.S | re.M)
    assert m, name
    body = "".join(f'    "{r}",\n' for r in rows)
    return text[:m.start(2)] + body + text[m.end(2):]


def tup(name, rows):
    return f"{name} = (\n" + "".join(f'    "{r}",\n' for r in rows) + ")\n"


# --- ハンプシャー・増毛: 足（と毛）が変わっただけなので、採用案の絵を実際のスプライトに合わせる ---
p = "docs/sprite-candidates/hampshire/candidates.txt"
t = read(p)
t = set_rows(t, "HAMPSHIRE_OUTLINE2", forms["ADULT_HAMPSHIRE"]["frames"]["F"])
write(p, t)

p = "docs/sprite-candidates/fluffy/candidates.txt"
t = read(p)
for breed in ("MERINO", "CORRIEDALE", "LINCOLN", "SUFFOLK", "HAMPSHIRE"):
    t = set_rows(t, f"FLUFF_{breed}_HALO", forms[f"ADULT_{breed}_FLUFFY"]["frames"]["F"])
write(p, t)

# --- 若羊: これまでの採用案を旧デザインにして、新しい絵を足す ---
p = "docs/sprite-candidates/young/candidates.txt"
t = read(p)
NEW = [
    # (旧の名前, 旧の状態の文言, 新しい ID, 新しい名前, 特徴, 実際のフォーム)
    ("YOUNG_MOKO_M0", "採用: YOUNG_MOKO", "M6", "YOUNG_MOKO_M6",
     "あなたが描いた案。頭の毛玉があり、丸みのある輪郭の体と細い脚", "YOUNG_MOKO"),
    ("YOUNG_SUFFOLK_D2", "採用: YOUNG_SUFFOLK", "D3", "YOUNG_SUFFOLK_D3",
     "D2 の胴の下側を丸く塗り、あごの下に線を足した版（あなたが描いた案）", "YOUNG_SUFFOLK"),
    ("YOUNG_WILD_WD", "採用: YOUNG_WILD", "WE", "YOUNG_WILD_WE",
     "WD の脚（row 12・15・16）を変えた版（あなたが描いた案）", "YOUNG_WILD"),
]
add = ""
for old_name, old_status, sid, name, feat, form in NEW:
    old_line = f" — {old_status}\n{old_name} = ("
    assert t.count(old_line) == 1, old_name
    t = t.replace(old_line, f" — 旧デザイン（{sid} に差し替えた）\n{old_name} = (")
    add += f"\n# {sid}: {feat} — {old_status}\n" + tup(name, forms[form]["frames"]["F"])
write(p, t.rstrip("\n") + "\n" + add)
print("docs を同期した")
```

Run: `python3 /private/tmp/claude-501/-Users-naofumiikeda-repositories-mokoji/5ec93858-ae33-4d1c-88e1-013474ae94c6/scratchpad/sync_docs.py`
Expected: `docs を同期した`

- [ ] **Step 3: young の README を更新する**

`docs/sprite-candidates/young/README.md` を、次のとおり直す（python で文字列置換し、各置換は 1 か所だけ当たること）。

```python
p = "docs/sprite-candidates/young/README.md"
t = open(p, encoding="utf-8").read()
def rep(old, new):
    global t
    assert t.count(old) == 1, old[:50]
    t = t.replace(old, new)
rep("| YOUNG_MOKO | M0 | 頭の毛玉あり・顔は見える。足元は row 19 |",
    "| YOUNG_MOKO | M6 | 頭の毛玉があり、丸みのある輪郭の体と細い脚。足元は row 19 |")
rep("| YOUNG_SUFFOLK | D2 | 体は輪郭だけで、顔のディテールは無い。成体 SUFFOLK と同じ作り。D より胴の輪郭が細くなだらか |",
    "| YOUNG_SUFFOLK | D3 | 体は輪郭だけで、顔のディテールは無い。成体 SUFFOLK と同じ作り。D2 より胴の下側が丸い |")
rep("| YOUNG_WILD | WD | ベビーの小さな体・短い脚に、黒い顔と角の芽・耳を足した形。体がムフロンと別の絵 |",
    "| YOUNG_WILD | WE | ベビーの小さな体に、黒い顔と角の芽・耳を足した形。WD から脚を変えた。体がムフロンと別の絵 |")
rep("| M0 | サフォーク若羊の旧デザインをそのままコピー（頭の毛玉あり・顔は見える） | 採用: YOUNG_MOKO |",
    "| M0 | サフォーク若羊の旧デザインをそのままコピー（頭の毛玉あり・顔は見える） | 旧デザイン（M6 に差し替えた） |\n"
    "| M6 | あなたが描いた案。頭の毛玉があり、丸みのある輪郭の体と細い脚 | 採用: YOUNG_MOKO |")
rep("| D2 | D の胴の輪郭（row 11・13〜16）を、細くなだらかにして、脚をすっきりさせた版（あなたが描いた案） | 採用: YOUNG_SUFFOLK |",
    "| D2 | D の胴の輪郭（row 11・13〜16）を、細くなだらかにして、脚をすっきりさせた版（あなたが描いた案） | 旧デザイン（D3 に差し替えた） |\n"
    "| D3 | D2 の胴の下側を丸く塗り、あごの下に線を足した版（あなたが描いた案） | 採用: YOUNG_SUFFOLK |")
rep("| WD | ベビーの体（小さな体・短い脚）に、黒い顔と角の芽・耳を足した形 | 採用: YOUNG_WILD |",
    "| WD | ベビーの体（小さな体・短い脚）に、黒い顔と角の芽・耳を足した形 | 旧デザイン（WE に差し替えた） |\n"
    "| WE | WD の脚（row 12・15・16）を変えた版（あなたが描いた案） | 採用: YOUNG_WILD |")
rep("- あわせて、モコ系の若羊の足元（row 17）を、他のフォームと同じ row 19 にそろえた。",
    "- その後、若羊 3 種（モコ系 M6・サフォーク系 D3・ワイルド系 WE）を、新しい絵に差し替えた。成体の足も新しい形（棒 + 2 本の足）に\n"
    "  変えたので、成体の足元は row 20 になった（ベビー・若羊は row 19 のまま）。新しい絵は `candidates.txt` を貼って確認する。\n"
    "- あわせて、モコ系の若羊の足元（row 17）を、他のフォームと同じ row 19 にそろえた。")
open(p, "w", encoding="utf-8").write(t)
```

Run: 上のコードを `python3 - <<'EOF' … EOF` で実行する。
Expected: エラーなし（`AssertionError` が出たら、README の該当文言を `sed -n` で確認して、置換前の文字列を実際の行に合わせる）

- [ ] **Step 4: 再生成して、全確認コマンドとファームのビルドを流す**

Run: `python3 sprites_gen.py && python3 test/test_sprites.py 2>&1 | tail -2 && python3 test/test_sprite_baseline.py 2>&1 | tail -2 && python3 test/test_preview_data.py 2>&1 | tail -2 && (cd test && make test 2>&1 | tail -2)`
Expected: すべて `OK` / `all tests passed`

Run: `cmake -B build -DPICO_SDK_PATH=$HOME/pico-sdk > /dev/null 2>&1 && cmake --build build -j > /tmp/build_evo.log 2>&1; echo "exit=$?"; tail -1 /tmp/build_evo.log`
Expected: `exit=0` と `Built target mokoji`

- [ ] **Step 5: Commit**

```bash
git add sprites.py sprites.h oled_preview_data.js test/test_sprite_baseline.py docs/sprite-candidates docs/superpowers
git commit -m "若羊 3 種・成体 3 種の絵を差し替え、成体の足を新しい形にそろえる" -m "<変更の要約（絵の差し替え、足、足元のテスト、docs の同期）を日本語で>" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 5: 毛・角は成体だけ。若羊のメニューはベビーと同じ

**Files:**
- Modify: `test/test_game.cpp`（テストの直しと追加）
- Modify: `game.cpp`（`menuSlot` / `menuCount`、`tick` の毛・角の伸び、`evolveYoung` / `evolveAdult` の毛・角のリセットとメニューカーソル）

**Interfaces:**
- Consumes: 既存の `make_stage_game(Game::Stage)` / `make_adult_game(Game::Breed)`（`test/test_game.cpp` にある）
- Produces: 若羊の `menuCount()` は 5、成体は 6。`tick` の毛・角は `_stage == ADULT` のときだけ伸びる。`evolveYoung` / `evolveAdult` が `_wool` と `_horn` を 0 に戻す。`evolveAdult` が `_menu_cursor` を 0 に戻す

- [ ] **Step 1: 既存のテストを、新しいルールに合わせて直す**

`test/test_game.cpp` の次のテストを置き換える。

`test_menu_labels_for_young_and_adult`（若羊は 5 項目、毛刈り・角研ぎは成体）:

```cpp
static void test_menu_labels_for_young_and_adult() {
    // 毛刈り・角研ぎは成体だけ。ベビーと若羊のメニューには出ない（5 項目）。
    Game young = make_stage_game(Game::Stage::YOUNG_MOKO);
    assert(young.menuCount() == 5);
    assert(std::string(young.menuLabel(2)) == "ゲーム");
    assert(young.menuAction(2) == Game::Action::MINI);

    Game moko = make_adult_game(Game::Breed::CORRIEDALE);
    assert(moko.menuCount() == 6);
    assert(std::string(moko.menuLabel(0)) == "ごはん");    // FEED
    assert(std::string(moko.menuLabel(1)) == "なでる");    // PET
    assert(std::string(moko.menuLabel(2)) == "毛刈り");    // SHEAR
    assert(std::string(moko.menuLabel(3)) == "ゲーム");    // MINI
    assert(std::string(moko.menuLabel(4)) == "プロフ");    // PROFILE
    assert(std::string(moko.menuLabel(5)) == "もどる");    // BACK
    assert(moko.menuAction(2) == Game::Action::SHEAR);
    assert(moko.menuAction(4) == Game::Action::PROFILE);
}
```

`test_baby_menu_has_no_shear`（ベビーと若羊 3 種で同じ 5 項目）:

```cpp
static void test_baby_and_young_menu_have_no_shear_or_polish() {
    const Game::Stage stages[] = { Game::Stage::BABY, Game::Stage::YOUNG_MOKO,
                                   Game::Stage::YOUNG_SUFFOLK, Game::Stage::YOUNG_WILD };
    for (Game::Stage s : stages) {
        Game g = make_stage_game(s);
        assert(g.menuCount() == 5);
        assert(std::string(g.menuLabel(0)) == "ごはん");
        assert(std::string(g.menuLabel(1)) == "なでる");
        assert(std::string(g.menuLabel(2)) == "ゲーム");
        assert(std::string(g.menuLabel(3)) == "プロフ");
        assert(std::string(g.menuLabel(4)) == "もどる");
        assert(*g.menuLabel(5) == '\0');                    // 5 項目目より先は空
        assert(g.menuAction(0) == Game::Action::FEED);
        assert(g.menuAction(1) == Game::Action::PET);
        assert(g.menuAction(2) == Game::Action::MINI);
        assert(g.menuAction(3) == Game::Action::PROFILE);
        assert(g.menuAction(4) == Game::Action::NONE);       // もどる
        assert(g.menuAction(5) == Game::Action::NONE);
        for (int i = 0; i < Game::MENU_COUNT; ++i) {
            assert(g.menuAction(i) != Game::Action::SHEAR);
            assert(g.menuAction(i) != Game::Action::POLISH);
        }
    }
}
```

`test_baby_wool_does_not_grow_but_young_does`（成体だけが伸ばす）:

```cpp
static void test_wool_and_horn_grow_only_for_adults() {
    // 毛・角は、毛刈り・角研ぎができる成体だけ伸ばす（若羊で伸ばすと、進化した時点で溜まったままになる）
    const Game::Stage young[] = { Game::Stage::BABY, Game::Stage::YOUNG_MOKO,
                                  Game::Stage::YOUNG_SUFFOLK, Game::Stage::YOUNG_WILD };
    for (Game::Stage s : young) {
        Game g = make_stage_game(s);
        for (uint32_t i = 0; i < 10 * Game::TICKS_PER_HOUR; ++i) g.tick();
        assert(g.wool() == 0);
        assert(g.horn() == 0);
    }
    Game moko = make_adult_game(Game::Breed::CORRIEDALE);
    for (uint32_t i = 0; i < 10 * Game::TICKS_PER_HOUR; ++i) moko.tick();
    assert(moko.wool() == 20);                           // 成体は 1 時間に +2
    assert(moko.horn() == 0);
    Game wild = make_adult_game(Game::Breed::MOUFLON);
    for (uint32_t i = 0; i < 10 * Game::TICKS_PER_HOUR; ++i) wild.tick();
    assert(wild.horn() == 20);
    assert(wild.wool() == 0);
}
```

`test_evolution_resets_menu_cursor`（若羊 → 成体で、項目数が 5 → 6 に変わる）と、進化での毛・角のリセット:

```cpp
// 進化の直前（あと 1 tick で進化する）の Game を作る。毛・角は古いセーブで溜まっている想定にする。
static Game make_about_to_evolve(Game::Stage stage, uint32_t days) {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage     = uint8_t(stage);
    d.wool      = 70;
    d.horn      = 70;
    d.age_ticks = days * Game::TICKS_PER_DAY - 1;
    return Game(nullptr, &d);
}

static void test_evolution_resets_menu_cursor_when_item_count_changes() {
    // 若羊 → 成体で、項目数が 5 → 6 に変わるので、開いたまま進化してもカーソルの意味がずれないよう先頭へ戻す
    Game g = make_about_to_evolve(Game::Stage::YOUNG_MOKO, 7);
    g.onButton(Game::Button::CENTER);                    // メニューを開く
    for (int i = 0; i < 4; ++i) g.onButton(Game::Button::RIGHT);
    assert(g.menuCursor() == 4);                         // 若羊のメニューの末尾（もどる）
    g.tick();                                            // 7 日めに入り、成体へ進化
    assert(g.stage() == Game::Stage::ADULT);
    assert(g.menuCount() == 6);
    assert(g.menuCursor() == 0);
}

static void test_evolution_clears_wool_and_horn() {
    Game young = make_about_to_evolve(Game::Stage::BABY, 3);
    young.tick();                                        // ベビー → 若羊
    assert(young.stage() != Game::Stage::BABY);
    assert(young.wool() == 0 && young.horn() == 0);
    Game adult = make_about_to_evolve(Game::Stage::YOUNG_WILD, 7);
    adult.tick();                                        // 若羊 → 成体
    assert(adult.stage() == Game::Stage::ADULT);
    assert(adult.wool() == 0 && adult.horn() == 0);
}
```

`test_menu_label_polish_for_wild`（成体のワイルド系）:

```cpp
static void test_menu_label_polish_for_wild() {
    Game g = make_adult_game(Game::Breed::MOUFLON);
    assert(g.family() == Game::Family::WILD);
    assert(std::string(g.menuLabel(2)) == "角研ぎ");   // POLISH（ワイルド系は毛ではなく角）
    assert(g.menuAction(2) == Game::Action::POLISH);
}
```

`test_menu_labels_renderable_and_bounded` の、`wild` と `moko` の作り方を、成体に変える（`d.stage = YOUNG_WILD` の 3 行を消し、次に置き換える）:

```cpp
    Game wild = make_adult_game(Game::Breed::MOUFLON);
    Game baby(nullptr);
    skip_naming(baby);
    Game moko = make_adult_game(Game::Breed::CORRIEDALE);
```
（その前の `Game base(nullptr); skip_naming(base); GameSaveData d = base.saveData();` は不要なので消す。`make_adult_game` は `test_kind_names` の付近で定義されているので、使う前に届くよう、定義を `make_stage_game` の直後へ移す。）

`make_sleeping_game` に品種の引数を足す:

```cpp
static Game make_sleeping_game(Game::Stage stage = Game::Stage::BABY, Game::Breed breed = Game::Breed::NONE) {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage    = uint8_t(stage);
    d.breed    = uint8_t(breed);
    d.sleeping = 1;
    d.sleepy   = 90;
    d.hunger   = 50;
    d.happy    = 50;
    d.wool     = 50;
    return Game(nullptr, &d);
}
```

`test_sleeping_other_actions_are_rejected` の Game の作り方を、成体に変える:

```cpp
        // 毛刈りは成体のメニューにだけあるので、成体で確認する
        Game g = act == Game::Action::SHEAR
                     ? make_sleeping_game(Game::Stage::ADULT, Game::Breed::CORRIEDALE)
                     : make_sleeping_game(Game::Stage::BABY);
```

`make_young_with_growth` を、成体に変える（`test_action_sounds_are_deferred` が使う）:

```cpp
static Game make_adult_with_growth(Game::Breed breed) {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage = uint8_t(Game::Stage::ADULT);
    d.breed = uint8_t(breed);
    d.wool  = 50;
    d.horn  = 50;
    return Game(nullptr, &d);
}
```
`test_action_sounds_are_deferred` の `make_young_with_growth(Game::Stage::YOUNG_MOKO)` を `make_adult_with_growth(Game::Breed::CORRIEDALE)` に、`make_young_with_growth(Game::Stage::YOUNG_WILD)` を `make_adult_with_growth(Game::Breed::MOUFLON)` に変える。`test_no_sound_when_action_has_no_effect` の `make_stage_game(Game::Stage::YOUNG_MOKO)` を `make_adult_game(Game::Breed::CORRIEDALE)` に変える。

`main()` の `RUN(...)` を、名前が変わったテストに合わせて直し、新しいテストを足す:
`RUN(test_baby_menu_has_no_shear)` → `RUN(test_baby_and_young_menu_have_no_shear_or_polish)`、
`RUN(test_baby_wool_does_not_grow_but_young_does)` → `RUN(test_wool_and_horn_grow_only_for_adults)`、
`RUN(test_evolution_resets_menu_cursor)` → `RUN(test_evolution_resets_menu_cursor_when_item_count_changes)` と `RUN(test_evolution_clears_wool_and_horn)`。

- [ ] **Step 2: 失敗を確認する**

Run: `cd test && make test 2>&1 | tail -6`
Expected: `Assertion failed` で止まる（若羊のメニューが 6 項目のまま、若羊で毛が伸びる、進化で毛が 0 に戻らない、など）。

- [ ] **Step 3: 実装する（`game.cpp`）**

`menuSlot` と `menuCount` を、成体だけ毛刈り・角研ぎの枠を出す形に直す:

```cpp
namespace {
// 表示位置 i を、既定のメニュー定義（kMenu*Default）の添字に直す。
// 毛刈り・角研ぎ（添字 2）は成体だけ。ベビーと若羊は 2 番目以降を 1 つ後ろへずらして、枠を出さない。
int menuSlot(Game::Stage stage, int i) {
    return (stage != Game::Stage::ADULT && i >= 2) ? i + 1 : i;
}
}  // namespace
```

```cpp
int Game::menuCount() const {
    return (_stage == Stage::ADULT) ? MENU_COUNT : MENU_COUNT - 1;
}
```

`tick()` の毛・角の伸びを、置き換える（「系統に応じて毛 or 角を伸ばす」から `}` までのブロック）:

```cpp
        // 毛・角は成体だけ伸ばす（毛刈り・角研ぎができるのは成体だけ）。
        // ベビー・若羊で伸ばすと、成体に進化した時点で毛・角が溜まったままになる。
        // 系統別の手入れ対象だけ伸びる: ワイルド系は角、それ以外（モコ・サフォーク系）は毛。
        if (_stage == Stage::ADULT) {
            constexpr int ADULT_GROW = 2;
            if (family() == Family::WILD) {
                _horn = std::min(100, _horn + ADULT_GROW);
            } else {
                _wool = std::min(100, _wool + ADULT_GROW);
            }
        }
```

`evolveYoung()` の先頭側の `_wool = 0;`（ワイルド系だけ）を消し、末尾を、毛・角のリセットに直す（メニューの項目数はベビー → 若羊で変わらないので、カーソルのリセットは要らない）:

```cpp
    _wool = 0;
    _horn = 0;   // 若羊は毛・角を伸ばさない。ベビーの間に溜まった分（古いセーブ）は持ち越さない
    _dirty = true;
    queueSfx(Sfx::HAPPY);
```
（`_menu_cursor = 0;` とそのコメント行 2 行は消す。）

`evolveAdult()` の `_stage = Stage::ADULT;` の直後に足す:

```cpp
    _wool = 0;
    _horn = 0;           // 若羊の間に溜まった分（古いセーブ）は持ち越さない
    _menu_cursor = 0;    // メニューの項目数が 5 → 6 に変わるので、開いたままでもカーソルの意味がずれないよう先頭へ
```

- [ ] **Step 4: 通ることを確認する**

Run: `cd test && make test 2>&1 | tail -3`
Expected: `=== all tests passed ===`

- [ ] **Step 5: Commit**

```bash
git add game.cpp test/test_game.cpp
git commit -m "毛・角の伸びと毛刈り・角研ぎを成体だけにし、若羊のメニューをベビーと同じにする" -m "<変更の要約を日本語で>" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 6: 進化を空腹・幸福の値と乱数で決める。`tend_*` を削除する

**Files:**
- Modify: `game.h`（`EvoChoice` と 3 つの static 関数を足し、`_tend_*` を消す）
- Modify: `game.cpp`（重みの関数、`evolveYoung` / `evolveAdult`、`tick` の減衰、`doAction` の加算、読み込み・`newGame`・`saveData` の `tend_*`）
- Modify: `save.h`（`tend_*` の注釈）
- Modify: `test/test_game.cpp`（重み・抽選・セーブのテスト）

**Interfaces:**
- Consumes: Task 5 で直した `evolveYoung` / `evolveAdult`
- Produces（`game.h` の `Game` の public）:
  - `struct EvoChoice { Stage stage; Breed breed; int weight; };`
  - `static void youngChoices(int hunger, int happy, EvoChoice out[3]);`（要素は モコ系・サフォーク系・ワイルド系 の順）
  - `static int adultChoices(Stage young, int hunger, int happy, EvoChoice out[2]);`（若羊の段階から、その系統の 2 品種を書く。若羊でなければ 0 を返す）
  - `static int pickWeighted(const EvoChoice* choices, int n, uint32_t roll);`（`roll % 重みの合計` で選ぶ。合計が 0 以下なら 0）

- [ ] **Step 1: 失敗するテストを書く（`test/test_game.cpp`）**

`test_evolution_sound_is_deferred` の後ろに、次を足す。

```cpp
// ---- 進化の重み（空腹・幸福の値だけで決め、残りは乱数）--------------------------
// 重みは、乱数と切り離した純粋関数にしてあるので、テストで直接確かめる。
static void test_young_choice_weights() {
    Game::EvoChoice c[3];
    Game::youngChoices(100, 100, c);             // 満腹で幸福
    assert(c[0].stage == Game::Stage::YOUNG_MOKO    && c[0].weight == 90);   // 50 + 100 * 40%
    assert(c[1].stage == Game::Stage::YOUNG_SUFFOLK && c[1].weight == 90);   // 50 + 100 * 40%
    assert(c[2].stage == Game::Stage::YOUNG_WILD    && c[2].weight == 50);   // 50 + (200 - 200) * 20%
    Game::youngChoices(0, 0, c);                 // 空腹で不幸: ワイルド系が一番出やすい
    assert(c[0].weight == 50 && c[1].weight == 50 && c[2].weight == 90);
    Game::youngChoices(100, 0, c);               // 満腹だけ: モコ系
    assert(c[0].weight == 90 && c[1].weight == 50 && c[2].weight == 70);
    Game::youngChoices(0, 100, c);               // 幸福だけ: サフォーク系
    assert(c[0].weight == 50 && c[1].weight == 90 && c[2].weight == 70);
    for (int i = 0; i < 3; ++i) assert(c[i].breed == Game::Breed::NONE);
}

static void test_choice_weights_stay_in_range_for_out_of_range_values() {
    // 範囲外の値（負・100 超）でも、重みが基本値（50）より小さくならず、上限を超えない
    Game::EvoChoice c[3];
    Game::youngChoices(-30, 250, c);
    for (int i = 0; i < 3; ++i) assert(c[i].weight >= 50 && c[i].weight <= 90);
    Game::EvoChoice a[2];
    assert(Game::adultChoices(Game::Stage::YOUNG_SUFFOLK, 300, -10, a) == 2);
    for (int i = 0; i < 2; ++i) assert(a[i].weight >= 50 && a[i].weight <= 100);
}

static void test_adult_choice_weights() {
    Game::EvoChoice c[2];
    // モコ系: CORRIEDALE / LINCOLN を半々（MERINO は抽選に入らない）
    assert(Game::adultChoices(Game::Stage::YOUNG_MOKO, 100, 100, c) == 2);
    assert(c[0].breed == Game::Breed::CORRIEDALE && c[0].weight == 50);
    assert(c[1].breed == Game::Breed::LINCOLN    && c[1].weight == 50);
    for (int i = 0; i < 2; ++i) assert(c[i].stage == Game::Stage::ADULT);
    // サフォーク系: 幸福なほど HAMPSHIRE が出やすい
    assert(Game::adultChoices(Game::Stage::YOUNG_SUFFOLK, 100, 100, c) == 2);
    assert(c[0].breed == Game::Breed::SUFFOLK   && c[0].weight == 50);
    assert(c[1].breed == Game::Breed::HAMPSHIRE && c[1].weight == 100);      // 50 + 100 * 50%
    Game::adultChoices(Game::Stage::YOUNG_SUFFOLK, 100, 0, c);
    assert(c[1].weight == 50);
    // ワイルド系: 満腹なほど BIGHORN が出やすい
    assert(Game::adultChoices(Game::Stage::YOUNG_WILD, 100, 100, c) == 2);
    assert(c[0].breed == Game::Breed::MOUFLON && c[0].weight == 50);
    assert(c[1].breed == Game::Breed::BIGHORN && c[1].weight == 100);
    Game::adultChoices(Game::Stage::YOUNG_WILD, 0, 100, c);
    assert(c[1].weight == 50);
    // 若羊でなければ、選択肢はない
    assert(Game::adultChoices(Game::Stage::BABY, 50, 50, c) == 0);
    assert(Game::adultChoices(Game::Stage::ADULT, 50, 50, c) == 0);
}

static void test_pick_weighted_uses_the_whole_roll() {
    Game::EvoChoice c[3] = {
        { Game::Stage::YOUNG_MOKO,    Game::Breed::NONE, 30 },
        { Game::Stage::YOUNG_SUFFOLK, Game::Breed::NONE, 20 },
        { Game::Stage::YOUNG_WILD,    Game::Breed::NONE, 50 },
    };
    assert(Game::pickWeighted(c, 3, 0)   == 0);
    assert(Game::pickWeighted(c, 3, 29)  == 0);
    assert(Game::pickWeighted(c, 3, 30)  == 1);
    assert(Game::pickWeighted(c, 3, 49)  == 1);
    assert(Game::pickWeighted(c, 3, 50)  == 2);
    assert(Game::pickWeighted(c, 3, 99)  == 2);
    assert(Game::pickWeighted(c, 3, 100) == 0);                     // roll は、合計で割った余りで使う
    assert(Game::pickWeighted(c, 3, 4000000099u) == 2);             // 32 ビットの乱数の全範囲を使う
    Game::EvoChoice zero[2] = {
        { Game::Stage::ADULT, Game::Breed::MOUFLON, 0 },
        { Game::Stage::ADULT, Game::Breed::BIGHORN, 0 },
    };
    assert(Game::pickWeighted(zero, 2, 12345) == 0);                // 合計が 0 なら 0 番目
}

// ---- 進化の結果 ----------------------------------------------------------------
static Game make_evolving(Game::Stage stage, uint32_t days, int hunger, int happy) {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage     = uint8_t(stage);
    d.hunger    = uint8_t(hunger);
    d.happy     = uint8_t(happy);
    d.age_ticks = days * Game::TICKS_PER_DAY - 1;
    return Game(nullptr, &d);
}

static void test_adult_evolution_never_picks_merino() {
    int corriedale = 0, lincoln = 0;
    for (int i = 0; i < 300; ++i) {
        Game g = make_evolving(Game::Stage::YOUNG_MOKO, 7, 100, 100);
        g.tick();
        assert(g.stage() == Game::Stage::ADULT);
        assert(g.breed() != Game::Breed::MERINO);
        if (g.breed() == Game::Breed::CORRIEDALE) ++corriedale;
        if (g.breed() == Game::Breed::LINCOLN)    ++lincoln;
    }
    assert(corriedale > 0 && lincoln > 0);                // 2 品種とも出る（乱数で決まる）
    assert(corriedale + lincoln == 300);
}

static void test_young_evolution_follows_hunger_and_happy() {
    // 空腹・不幸なほどワイルド系が出やすく、満腹・幸福なほど出にくい（重み 88 対 50 なので、300 回なら十分に差が出る）
    int wild_hungry = 0, wild_happy = 0;
    for (int i = 0; i < 300; ++i) {
        Game low = make_evolving(Game::Stage::BABY, 3, 5, 5);
        low.tick();
        if (low.stage() == Game::Stage::YOUNG_WILD) ++wild_hungry;
        Game high = make_evolving(Game::Stage::BABY, 3, 100, 100);
        high.tick();
        if (high.stage() == Game::Stage::YOUNG_WILD) ++wild_happy;
    }
    assert(wild_hungry > wild_happy + 30);
}

static void test_young_evolution_can_pick_every_family() {
    // 抽選が乱数の全範囲を使うこと（0〜127 だけだと、最後の選択肢が選ばれにくい）
    int count[3] = {0, 0, 0};
    for (int i = 0; i < 600; ++i) {
        Game g = make_evolving(Game::Stage::BABY, 3, 100, 100);
        g.tick();
        if (g.stage() == Game::Stage::YOUNG_MOKO)    ++count[0];
        if (g.stage() == Game::Stage::YOUNG_SUFFOLK) ++count[1];
        if (g.stage() == Game::Stage::YOUNG_WILD)    ++count[2];
    }
    assert(count[0] + count[1] + count[2] == 600);
    assert(count[2] > 60);                                 // ワイルド系（重み 50 / 合計 230）も、ちゃんと出る
}

// ---- セーブ形式は変えない -------------------------------------------------------
static void test_old_save_with_tend_values_still_loads() {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.tend_feed = 20; d.tend_pet = 30; d.tend_shear = 40; d.tend_polish = 50;   // 古いセーブの値
    Game g(nullptr, &d);
    assert(g.stage() == base.stage());                    // 読み込める
    GameSaveData out = g.saveData();
    assert(out.tend_feed == 0 && out.tend_pet == 0 && out.tend_shear == 0 && out.tend_polish == 0);
    assert(out.magic == 0x4D4F4B35u);                     // MAGIC は変えない（今の羊のセーブを消さない）
}
```

`test_minigame_reward_capped_and_applied_once`（付近）の、次の 2 行を消す（`tend_*` はもう使わないので、比べる意味がない）:

```cpp
    assert(after.tend_feed == before.tend_feed && after.tend_pet == before.tend_pet);
    assert(after.tend_shear == before.tend_shear && after.tend_polish == before.tend_polish);
```
（`before` / `after` が他で使われていなければ、その宣言も、警告が出ない範囲で整理する。）

`main()` の `RUN(test_evolution_sound_is_deferred);` の後ろに足す:

```cpp
    RUN(test_young_choice_weights);
    RUN(test_choice_weights_stay_in_range_for_out_of_range_values);
    RUN(test_adult_choice_weights);
    RUN(test_pick_weighted_uses_the_whole_roll);
    RUN(test_adult_evolution_never_picks_merino);
    RUN(test_young_evolution_follows_hunger_and_happy);
    RUN(test_young_evolution_can_pick_every_family);
    RUN(test_old_save_with_tend_values_still_loads);
```

- [ ] **Step 2: 失敗を確認する**

Run: `cd test && make test 2>&1 | tail -8`
Expected: コンパイルエラー（`youngChoices` / `adultChoices` / `pickWeighted` / `EvoChoice` が未定義）

- [ ] **Step 3: `game.h` を直す**

`Family` の enum の直後に足す:

```cpp
    // 進化の候補と重み。進化のときの空腹（hunger）・幸福（happy）の値（0〜100）だけで重みを決め、
    // 残りは乱数で選ぶ。乱数と切り離した純粋関数にしてあるので、テストで重みを直接確かめられる。
    struct EvoChoice {
        Stage stage;    // 進化後の成長段階
        Breed breed;    // 進化後の品種（ベビー → 若羊では NONE）
        int   weight;   // 選ばれやすさ
    };
    // ベビー → 若羊。モコ系・サフォーク系・ワイルド系の 3 つ（この順）を out に書く。
    static void youngChoices(int hunger, int happy, EvoChoice out[3]);
    // 若羊 → 成体。若羊の段階（YOUNG_*）から、その系統の 2 品種を out に書いて、個数を返す（若羊でなければ 0）。
    static int  adultChoices(Stage young, int hunger, int happy, EvoChoice out[2]);
    // 重みに従って 1 つ選ぶ。roll は乱数（roll % 重みの合計 で選ぶ）。合計が 0 以下なら 0 番目。
    static int  pickWeighted(const EvoChoice* choices, int n, uint32_t roll);
```

`private:` の `_tend_feed` 〜 `_tend_polish`（コメント込みで 4 行）を消す。

- [ ] **Step 4: `game.cpp` を直す**

無名 namespace（`menuSlot` の近く）に、定数と補助関数を足す:

```cpp
namespace {
// 進化の重み: 基本 50 に、空腹・幸福の値（0〜100）に応じた分を足す（係数はパーセント）。
constexpr int EVO_BASE_WEIGHT          = 50;
constexpr int EVO_MOKO_HUNGER_PCT      = 40;   // モコ系: 満腹なほど
constexpr int EVO_SUFFOLK_HAPPY_PCT    = 40;   // サフォーク系: 幸福なほど
constexpr int EVO_WILD_LOW_PCT         = 20;   // ワイルド系: 空腹・不幸なほど（(200 − hunger − happy) に掛ける）
constexpr int EVO_HAMPSHIRE_HAPPY_PCT  = 50;   // HAMPSHIRE: 幸福なほど
constexpr int EVO_BIGHORN_HUNGER_PCT   = 50;   // BIGHORN: 満腹なほど

int clampPercent(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
}  // namespace
```

`Game::menuAction` の後ろに、3 つの関数を足す:

```cpp
void Game::youngChoices(int hunger, int happy, EvoChoice out[3]) {
    const int h = clampPercent(hunger);
    const int p = clampPercent(happy);
    out[0] = { Stage::YOUNG_MOKO,    Breed::NONE, EVO_BASE_WEIGHT + h * EVO_MOKO_HUNGER_PCT / 100 };
    out[1] = { Stage::YOUNG_SUFFOLK, Breed::NONE, EVO_BASE_WEIGHT + p * EVO_SUFFOLK_HAPPY_PCT / 100 };
    // ワイルド系は「放置気味」（空腹・不幸なほど出やすい）。満腹・幸福の合計が低いほど、重みが増える
    out[2] = { Stage::YOUNG_WILD,    Breed::NONE, EVO_BASE_WEIGHT + (200 - h - p) * EVO_WILD_LOW_PCT / 100 };
}

int Game::adultChoices(Stage young, int hunger, int happy, EvoChoice out[2]) {
    const int h = clampPercent(hunger);
    const int p = clampPercent(happy);
    switch (young) {
        case Stage::YOUNG_MOKO:
            // MERINO は、一旦、抽選に入れない（絵と Breed は残してある）
            out[0] = { Stage::ADULT, Breed::CORRIEDALE, EVO_BASE_WEIGHT };
            out[1] = { Stage::ADULT, Breed::LINCOLN,    EVO_BASE_WEIGHT };
            return 2;
        case Stage::YOUNG_SUFFOLK:
            out[0] = { Stage::ADULT, Breed::SUFFOLK,   EVO_BASE_WEIGHT };
            out[1] = { Stage::ADULT, Breed::HAMPSHIRE, EVO_BASE_WEIGHT + p * EVO_HAMPSHIRE_HAPPY_PCT / 100 };
            return 2;
        case Stage::YOUNG_WILD:
            out[0] = { Stage::ADULT, Breed::MOUFLON, EVO_BASE_WEIGHT };
            out[1] = { Stage::ADULT, Breed::BIGHORN, EVO_BASE_WEIGHT + h * EVO_BIGHORN_HUNGER_PCT / 100 };
            return 2;
        default:
            return 0;
    }
}

int Game::pickWeighted(const EvoChoice* choices, int n, uint32_t roll) {
    int total = 0;
    for (int i = 0; i < n; ++i) total += choices[i].weight;
    if (total <= 0) return 0;
    int r = int(roll % uint32_t(total));       // 32 ビットの乱数をそのまま使う（127 までしか使わないと、後ろの選択肢が偏る）
    for (int i = 0; i < n; ++i) {
        if (r < choices[i].weight) return i;
        r -= choices[i].weight;
    }
    return n - 1;
}
```

`evolveYoung()` の本体（コメントから `queueSfx` の前まで）を、置き換える:

```cpp
void Game::evolveYoung() {
    // BABY → 3 系統。進化のときの空腹・幸福の値で重み付けし、残りは乱数（重みは youngChoices を参照）。
    EvoChoice c[3];
    youngChoices(_hunger, _happy, c);
    _stage = c[pickWeighted(c, 3, get_rand_32())].stage;
    _wool = 0;
    _horn = 0;   // 若羊は毛・角を伸ばさない。ベビーの間に溜まった分（古いセーブ）は持ち越さない
    _dirty = true;
    queueSfx(Sfx::HAPPY);
}
```

`evolveAdult()` の本体を、置き換える:

```cpp
void Game::evolveAdult() {
    // 若羊 → 品種。進化のときの空腹・幸福の値で重み付けし、残りは乱数（重みは adultChoices を参照）。
    EvoChoice c[2];
    const int n = adultChoices(_stage, _hunger, _happy, c);
    _breed = c[pickWeighted(c, n, get_rand_32())].breed;

    _stage = Stage::ADULT;
    _wool = 0;
    _horn = 0;           // 若羊の間に溜まった分（古いセーブ）は持ち越さない
    _menu_cursor = 0;    // メニューの項目数が 5 → 6 に変わるので、開いたままでもカーソルの意味がずれないよう先頭へ
    _dirty = true;
    queueSfx(Sfx::HAPPY);
}
```
（Task 5 で足した `_wool = 0;` などの行と重複しないように、本体は 1 つにまとめる。）

`tend_*` を消す:
- `tick()` の「傾向スコアは ×0.97/hour で減衰」のコメントと 4 行を、消す。
- `doAction()` の `_tend_feed += 1;`（FEED）、`_tend_pet += 1;`（PET）、`_tend_shear += 1;`（SHEAR）、`_tend_polish += 1;`（POLISH）を、消す。
- コンストラクタの読み込み（`_tend_feed = data->tend_feed;` など 4 行）と、`newGame()` の `_tend_* = 0;` 4 行を、消す。
- `saveData()` の `d.tend_* = ...` 4 行を消す（`GameSaveData d{}` で 0 になる）。
- `applyMiniReward` の上のコメント `// 進化の傾向スコア（tend_*）には反映しない。` を、`// 幸福度は上がる（幸福度は進化の重みに使う値でもある）。` に直す。

- [ ] **Step 5: `save.h` の注釈を直す**

`tend_feed` 〜 `tend_polish` の 4 行の上に、次のコメントを足す（構造は変えない）:

```cpp
    // 以下の 4 つ（tend_*）は、もう進化の判定に使わない。セーブ形式を変えない（今の羊のセーブを消さない）ために
    // 領域だけ残し、保存では 0 を書き、読み込みでは無視する。
```

- [ ] **Step 6: 通ることを確認する**

Run: `cd test && make test 2>&1 | tail -3`
Expected: `=== all tests passed ===`（警告が出ていないことも見る）

- [ ] **Step 7: Commit**

```bash
git add game.h game.cpp save.h test/test_game.cpp
git commit -m "進化を空腹・幸福の値と乱数で決め、傾向スコア（tend_*）を使わない" -m "<変更の要約（重み、MERINO を抽選から外す、抽選が 0〜127 しか使わなかった不具合の修正、セーブ形式は不変）を日本語で>" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 7: 増毛のキラキラを止める

**Files:**
- Modify: `display.cpp`（`drawWool` の呼び出しと関数を削除）
- Modify: `display.h`（宣言を削除）

**Interfaces:**
- Consumes: なし
- Produces: メイン画面に、毛のキラキラが出ない

- [ ] **Step 1: 削除する**

`display.cpp` の `drawMain` から、次のコメントと呼び出しを消す:

```cpp
    // 毛キラキラはモコ・サフォーク系のみ（ワイルド系は wool 概念がないため非表示）。
    // 旧 save / BABY 時代に溜まった wool が YOUNG_WILD に持ち越されてもキラキラを出さない。
    if (g.wool() > 30 && g.family() != Game::Family::WILD) {
        drawWool(g.wool(), sx);
    }
```
`display.cpp` の `void Display::drawWool(int wool, int sx) { … }` の関数全体を消す。
`display.h` の `void drawWool(int wool, int sx);` を消す。

- [ ] **Step 2: 参照が残っていないことと、ビルドを確認する**

Run: `grep -n "drawWool" display.cpp display.h main.cpp || echo "(参照なし)"`
Expected: `(参照なし)`

Run: `cmake --build build -j > /tmp/build_evo.log 2>&1; echo "exit=$?"; tail -1 /tmp/build_evo.log; (cd test && make test 2>&1 | tail -1)`
Expected: `exit=0`、`Built target mokoji`、`=== all tests passed ===`

- [ ] **Step 3: Commit**

```bash
git add display.cpp display.h
git commit -m "毛が伸びたときのキラキラのエフェクトを止める" -m "<変更の要約を日本語で>" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 8: CLAUDE.md と記憶を、新しいルールに合わせる

**Files:**
- Modify: `CLAUDE.md`（進化システムの節）
- Modify: `/Users/naofumiikeda/.claude/projects/-Users-naofumiikeda-repositories-mokoji/memory/project_mokoji_evolution_spec.md`（記憶。リポジトリ外）

**Interfaces:**
- Consumes: Task 5・6 のルール
- Produces: ドキュメントが実装と一致する

- [ ] **Step 1: `CLAUDE.md` を直す**

「進化システム」の節の、次の 4 行を置き換える。

```
進化判定は `tend_feed` / `tend_pet` / `tend_shear` / `tend_polish` の傾向スコアで決まる。
スコアは game-hour ごとに ×0.97 で減衰し、直近の世話が効く設計。
系統別にメニュー項目が切り替わる（モコ・サフォーク系: 毛刈り、ワイルド系: 角研ぎ）。
ベビーは系統がなく、毛刈りができない（メニューは 4 項目、毛も伸びない）。若羊から毛刈り・角研ぎが出る。
```

次の内容に置き換える。

```
進化判定は、進化のときの空腹（`hunger`）・幸福（`happy`）の値と乱数で決まる（重みは `Game::youngChoices` /
`adultChoices`。食べ物 → モコ系・BIGHORN、幸福度 → サフォーク系・HAMPSHIRE、どちらも低い → ワイルド系）。
世話の傾向スコア（`tend_*`）は使わない（セーブの領域だけ残し、0 を書く）。MERINO は、一旦、進化先から外している。
毛・角は成体だけ伸び、毛刈り・角研ぎ（モコ・サフォーク系: 毛刈り、ワイルド系: 角研ぎ）も成体のメニューにだけ出る。
ベビーと若羊のメニューは 5 項目（ごはん・なでる・ゲーム・プロフ・もどる）。
```

- [ ] **Step 2: 記憶を直す**

`project_mokoji_evolution_spec.md` の「お世話アクション仕様」の表の下に、次の節を足す（見出しは `**進化判定と毛・角（2026-09-21 に変更）：**`）:

```
**進化判定と毛・角（2026-09-21 に変更）：**
- 進化判定は、進化のときの hunger（空腹バー）・happy（幸福バー）の値と乱数だけ。世話の傾向スコア（tend_*）は使わない。
- 毛・角は成体だけ伸び、毛刈り・角研ぎも成体だけ。ベビー・若羊は 5 項目のメニュー。増毛のキラキラは止めた。
- MERINO は、一旦、進化先から外した（Breed・絵は残す）。
- 成体の足は新しい形（棒 + 2 本）で、足元は row 20（ベビー・若羊は row 19）。
```
（`MEMORY.md` の索引の 1 行は、そのままで足りる。）

- [ ] **Step 3: Commit（`CLAUDE.md` のみ。記憶はリポジトリ外）**

```bash
git add CLAUDE.md
git commit -m "CLAUDE.md の進化システムの説明を、新しいルールに合わせる" -m "<変更の要約を日本語で>" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 9: 増毛版の毛を大きくする（候補 → ユーザーが選ぶ → 実装）

**Files:**
- Modify: `sprites.py`（`_halo` の毛の幅）
- Modify: `test/test_sprites.py`（増毛のテスト）
- Modify: `docs/sprite-candidates/fluffy/`（採用案の記録）

**Interfaces:**
- Consumes: `_halo(base_rows)`（今は体の左右の端の外側に 1px の毛を足す。row 10〜17）
- Produces: 選ばれた毛の大きさ。`_halo` の引数 `width`（毛の幅 px）で表す

- [ ] **Step 1: 候補を作って、クリップボードにコピーする**

スクラッチ領域に、`_halo` の毛の幅を変えた候補を作るスクリプトを書く。

```python
"""増毛版の毛を大きくする候補（sprites.py は書き換えない）。CORRIEDALE と SUFFOLK と MOUFLON 系の 3 種で比べる。"""
import sys
sys.path.insert(0, ".")
import sprites, sprites_gen

W = 24


def halo(rows, width, rows_range=(10, 18)):
    out = [list(r) for r in rows]
    for y in range(*rows_range):
        xs = [i for i, c in enumerate(rows[y]) if c == "#"]
        if not xs:
            continue
        for d in range(1, width + 1):
            if xs[0] - d >= 0:
                out[y][xs[0] - d] = "#"
            if xs[-1] + d < W:
                out[y][xs[-1] + d] = "#"
    return ["".join(r) for r in out]


base = lambda n: sprites_gen.to_ascii(getattr(sprites, n + "_F"))
cands = []
for breed in ("ADULT_CORRIEDALE", "ADULT_SUFFOLK", "ADULT_MOUFLON"):
    b = base(breed)
    cands.append((f"{breed}_BASE", b))
    cands.append((f"{breed}_H1", halo(b, 1)))                    # 今（1px）
    cands.append((f"{breed}_H2", halo(b, 2)))                    # 2px
    cands.append((f"{breed}_H2R", halo(b, 2, (9, 18))))          # 2px + 耳の行（row 9）まで
    cands.append((f"{breed}_H3", halo(b, 3)))                    # 3px

print("  ".join(k[-10:].ljust(24) for k, _ in cands[:5]))
for y in range(8, 21):
    print("  ".join(r[y] for _, r in cands[:5]))
out = "".join(f"{n} = (\n" + "".join(f'    "{r}",\n' for r in rows) + ")\n\n" for n, rows in cands)
open("/private/tmp/claude-501/-Users-naofumiikeda-repositories-mokoji/5ec93858-ae33-4d1c-88e1-013474ae94c6/scratchpad/fluffy_big_out.txt", "w", encoding="utf-8").write(out)
```

Run: 上のスクリプトを実行し、`pbcopy < …/fluffy_big_out.txt` でクリップボードへコピーして、`pbpaste | grep -c '^[A-Z_0-9]* = ('` で個数を確かめる（15 個）。
Expected: 15 個。名前は `ADULT_<品種>_BASE` / `_H1` / `_H2` / `_H2R` / `_H3`。

- [ ] **Step 2: ユーザーに、候補を見せて、選んでもらう**

CORRIEDALE の 5 案（BASE・H1・H2・H2R・H3）を、チャットに並べて見せ、クリップボードにコピーしたことを伝える。**選ぶまで、次に進まない。**

- [ ] **Step 3: 選ばれた幅で、テストを先に書き換える（失敗を確認）**

選ばれた幅を `N` とする。`test/test_sprites.py` の `FluffySpriteTest` に、増毛版が通常版より `N` px 広いことを確かめるテストを足す。

```python
    def test_wool_is_as_wide_as_chosen(self):
        # 増毛版の毛は、体（row 10〜17）の左右の端の外側に N px。耳（row 9）と足元には足さない
        for name, base in FLUFFY_BASE.items():
            with self.subTest(sprite=name):
                rows, base_rows = grid(name), grid(base)
                for y in range(10, 18):
                    xs = [i for i, c in enumerate(base_rows[y]) if c == "#"]
                    if not xs:
                        continue
                    self.assertEqual(rows[y].index("#"), max(0, xs[0] - N), f"{name} row {y}: 左の毛の幅")
                    self.assertEqual(rows[y].rindex("#"), min(23, xs[-1] + N), f"{name} row {y}: 右の毛の幅")
                self.assertEqual(rows[9], base_rows[9], f"{name}: 耳の行には毛を足さない")
                self.assertEqual(rows[18:], base_rows[18:], f"{name}: 足元には毛を足さない")
```
（`N` は、選ばれた数値に置き換える。）

Run: `python3 test/test_sprites.py 2>&1 | grep -E "^(FAIL|OK|Ran)" | sed 's/(sprite=.*//' | sort | uniq -c`
Expected: `test_wool_is_as_wide_as_chosen` が失敗する（今は 1px）。

- [ ] **Step 4: `_halo` を、選ばれた幅にする**

`sprites.py` の `_halo` を、幅の引数つきにして、`FLUFFY` の 5 種の呼び出しを、その幅にする。

```python
def _halo(base_rows, width=1):
    """通常版 base_rows の体（row 10〜17）の左右の端の外側に、width px ずつ毛を足した rows tuple を返す（増毛版）。

    端は行ごとに検出するので、毛は必ず体につながり、通常版の絵は変わらない。
    毛は各行の左右の端に足すため、通常版の行が左右対称なら増毛版も対称になる。
    row 9（耳）と足元（row 18〜）には足さない。"""
    out = [list(r) for r in base_rows]
    for y in range(10, 18):
        xs = [i for i, c in enumerate(base_rows[y]) if c == '#']
        if not xs:
            continue
        for d in range(1, width + 1):
            if xs[0] - d >= 0:
                out[y][xs[0] - d] = '#'
            if xs[-1] + d < 24:
                out[y][xs[-1] + d] = '#'
    return tuple(''.join(row) for row in out)
```
`ADULT_*_FLUFFY_F` / `_L` の `_halo(X)` を、`_halo(X, width=N)` に変える（10 行）。

- [ ] **Step 5: 再生成して、全確認コマンドとビルドを流し、docs を同期する**

Run: `python3 sprites_gen.py && python3 /private/tmp/claude-501/-Users-naofumiikeda-repositories-mokoji/5ec93858-ae33-4d1c-88e1-013474ae94c6/scratchpad/sync_docs.py`
（`sync_docs.py` は Task 4 のもの。若羊の「旧デザインにして新しい絵を足す」部分は、2 回目の実行では、すでに旧デザインになっているので `AssertionError` になる。**増毛の採用案の同期（`hampshire` と `fluffy` の部分）だけを実行するよう、若羊のブロックを、`if "--young" in sys.argv:` で囲んで、Task 4 では `--young` つきで実行する。** ここでは `--young` なしで実行する。）

Run: `python3 test/test_sprites.py 2>&1 | tail -2; python3 test/test_sprite_baseline.py 2>&1 | tail -2; python3 test/test_preview_data.py 2>&1 | tail -2; (cd test && make test 2>&1 | tail -1); cmake --build build -j > /tmp/build_evo.log 2>&1; echo "exit=$?"`
Expected: すべて `OK` / `all tests passed`、`exit=0`

`docs/sprite-candidates/fluffy/README.md` の「採用の内容」（毛は体の左右の端の外側に 1px）を、選ばれた幅に直す。

- [ ] **Step 6: Commit**

```bash
git add sprites.py sprites.h oled_preview_data.js test/test_sprites.py docs/sprite-candidates
git commit -m "増毛版の毛を大きくする" -m "<変更の要約（毛の幅を 1px から N px に、テスト、docs）を日本語で>" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 10: 最終確認と PR

**Files:** なし

- [ ] **Step 1: 全確認コマンドと、両方のファームのビルド**

Run: `python3 test/test_sprites.py && python3 test/test_sprite_baseline.py && python3 test/test_preview_data.py && (cd test && make test)`
Expected: すべて通る。

Run: `cmake --build build -j > /tmp/build_evo.log 2>&1; echo "通常版 exit=$?"; cmake -B build_fast -DPICO_SDK_PATH=$HOME/pico-sdk -DDEBUG_FAST=ON > /dev/null 2>&1 && cmake --build build_fast -j > /tmp/build_evo_fast.log 2>&1; echo "進化確認用 exit=$?"`
Expected: どちらも `exit=0`

- [ ] **Step 2: PR を作る**

`/pr`（モード A）の手順で、`origin/main` の進みを確認して取り込み、push して、PR を作る。PR の本文には、次を書く: 進化ルールの変更（重みの表）、毛・角とメニュー、キラキラの停止、MERINO を抽選から外したこと、絵と足、足元の基準（成体 row 20）、セーブ形式は不変、抽選の乱数の範囲の不具合の修正、未確認の項目（実機の見た目、進化の体感）。
