#!/usr/bin/env python3
"""sprites.py の Python リストから sprites.h (C++ 配列) と oled_preview_data.js を生成する。

使い方:
    python3 sprites_gen.py            # sprites.h と oled_preview_data.js を上書き生成
    python3 sprites_gen.py preview    # sprites.py のスプライトを ASCII で確認

oled_preview_data.js は oled_preview.html が読み込む。成長段階・系統付きで全スプライトを持つので、
HTML を開くだけで進化ツリーが表示できる（sprites.py の貼り付け不要）。
docs/sprite-candidates/*/candidates.txt の候補（没案を含む）も一緒に持ち、没候補として表示できる。
"""

import json
import os
import re
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
    "ADULT_CORRIEDALE_F", "ADULT_CORRIEDALE_L", "ADULT_CORRIEDALE_R",
    "ADULT_LINCOLN_F",    "ADULT_LINCOLN_L",    "ADULT_LINCOLN_R",
    "ADULT_SUFFOLK_F",    "ADULT_SUFFOLK_L",    "ADULT_SUFFOLK_R",
    "ADULT_HAMPSHIRE_F",  "ADULT_HAMPSHIRE_L",  "ADULT_HAMPSHIRE_R",
    "ADULT_MOUFLON_F",    "ADULT_MOUFLON_L",    "ADULT_MOUFLON_R",
    "ADULT_BIGHORN_F",    "ADULT_BIGHORN_L",    "ADULT_BIGHORN_R",
    # Stage 4: SPECIAL（増毛 5 + 角長 2）
    "ADULT_CORRIEDALE_FLUFFY_F", "ADULT_CORRIEDALE_FLUFFY_L", "ADULT_CORRIEDALE_FLUFFY_R",
    "ADULT_LINCOLN_FLUFFY_F",    "ADULT_LINCOLN_FLUFFY_L",    "ADULT_LINCOLN_FLUFFY_R",
    "ADULT_SUFFOLK_FLUFFY_F",    "ADULT_SUFFOLK_FLUFFY_L",    "ADULT_SUFFOLK_FLUFFY_R",
    "ADULT_HAMPSHIRE_FLUFFY_F",  "ADULT_HAMPSHIRE_FLUFFY_L",  "ADULT_HAMPSHIRE_FLUFFY_R",
    "ADULT_MOUFLON_LONGHORN_F",  "ADULT_MOUFLON_LONGHORN_L",  "ADULT_MOUFLON_LONGHORN_R",
    "ADULT_BIGHORN_LONGHORN_F",  "ADULT_BIGHORN_LONGHORN_L",  "ADULT_BIGHORN_LONGHORN_R",
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


# === oled_preview.html 用データ（成長段階・系統） ===
# スプライト名は BABY / YOUNG_<系統> / ADULT_<品種> / ADULT_<品種>_FLUFFY|LONGHORN の形で、
# ここから段階と系統を割り出す。対応表は game.h の Stage / Breed / Family に合わせる。
# 新しい品種・系統を足したときは、この表も更新する（未知の名前は ValueError で気づける）。

# 成長段階（表示順）。ADULT の増毛期・角長期は、ゲーム側では ADULT のまま量で判定するが、
# プレビューでは別の段階 SPECIAL（特殊成長）として並べる。
STAGES = [
    ("BABY", "ベビー"),
    ("YOUNG", "若羊"),
    ("ADULT", "成体"),
    ("SPECIAL", "特殊成長"),
]

# 系統（表示順）
FAMILIES = [
    ("MOKO", "モコ系"),
    ("SUFFOLK", "サフォーク系"),
    ("WILD", "ワイルド系"),
]

# 品種 → (系統, 表示名)。game.h の Breed に対応
BREEDS = {
    "CORRIEDALE": ("MOKO",    "コリデール"),
    "LINCOLN":    ("MOKO",    "リンカーン"),
    "SUFFOLK":    ("SUFFOLK", "サフォーク"),
    "HAMPSHIRE":  ("SUFFOLK", "ハンプシャー"),
    "MOUFLON":    ("WILD",    "ムフロン"),
    "BIGHORN":    ("WILD",    "ビッグホーン"),
}

# 特殊成長 → (表示名, 対象の系統)。増毛はモコ・サフォーク系のみ、角長はワイルド系のみ
VARIANTS = {
    "FLUFFY":   ("増毛", {"MOKO", "SUFFOLK"}),
    "LONGHORN": ("角長", {"WILD"}),
}

FRAMES = ("F", "L", "R")   # 正面 / 左向き / 右向き


def classify_form(form_id):
    """フォーム名（_F/_L/_R を除いた名前）から段階・系統・品種・特殊成長を割り出す。

    未知の名前や組み合わせは ValueError（対応表の更新漏れを黙って通さない）。
    """
    family_labels = dict(FAMILIES)
    parts = form_id.split("_")

    if form_id == "BABY":
        return {"id": form_id, "stage": "BABY", "family": None, "breed": None,
                "variant": None, "label": "ベビー"}

    if len(parts) == 2 and parts[0] == "YOUNG" and parts[1] in family_labels:
        return {"id": form_id, "stage": "YOUNG", "family": parts[1], "breed": None,
                "variant": None, "label": f"若羊 {family_labels[parts[1]]}"}

    if parts[0] == "ADULT" and len(parts) in (2, 3) and parts[1] in BREEDS:
        breed = parts[1]
        family, breed_label = BREEDS[breed]
        if len(parts) == 2:
            return {"id": form_id, "stage": "ADULT", "family": family, "breed": breed,
                    "variant": None, "label": breed_label}
        variant = parts[2]
        if variant in VARIANTS and family in VARIANTS[variant][1]:
            return {"id": form_id, "stage": "SPECIAL", "family": family, "breed": breed,
                    "variant": variant, "label": f"{breed_label} {VARIANTS[variant][0]}"}

    raise ValueError(f"段階・系統を判定できないスプライト名: {form_id}（sprites_gen.py の対応表を更新する）")


def forms():
    """SPRITE_NAMES を（_F/_L/_R を束ねた）フォーム単位にまとめる。SPRITE_NAMES の並び順を保つ。"""
    grouped = {}
    for name in SPRITE_NAMES:
        form_id, _, frame = name.rpartition("_")
        if frame not in FRAMES:
            raise ValueError(f"末尾が _F/_L/_R ではないスプライト名: {name}")
        if form_id not in grouped:
            grouped[form_id] = classify_form(form_id)
            grouped[form_id]["frames"] = {}
        grouped[form_id]["frames"][frame] = to_ascii(getattr(sprites, name))
    for form_id, f in grouped.items():
        missing = [k for k in FRAMES if k not in f["frames"]]
        if missing:
            raise ValueError(f"{form_id} に {missing} のフレームがない")
    return list(grouped.values())


# === 候補（docs/sprite-candidates） ===
# 各フォルダの candidates.txt は、oled_preview.html にそのまま貼れる形式（NAME = ( "24 文字" x 24 行 )）で、
# 各スプライトの直前に「# <ID>: <特徴> — <状態>」のコメントがある。それを読んで、没候補などとして表示する。

CANDIDATES_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "docs", "sprite-candidates")

# トピック（フォルダ名, 表示名）。表示順。docs に新しいフォルダを足したら、ここにも足す。
TOPICS = [
    ("young", "若羊"),
    ("fluffy", "増毛"),
    ("horn", "角（ワイルド系）"),
    ("hampshire", "ハンプシャー"),
    ("merino", "メリノ（進化先から外した）"),
]


def candidate_kind(status):
    """候補の状態の文言を、adopted（採用）/ rejected（没: 未採用・外した・旧デザイン）/ other（B 案のベースなど）に分ける。

    未知の言い回しは ValueError（分類の更新漏れを黙って通さない）。
    """
    if status.startswith("採用"):
        return "adopted"
    if any(k in status for k in ("未採用", "外した", "旧デザイン")):
        return "rejected"
    if "ベースに" in status:
        return "other"
    raise ValueError(f"候補の状態を分類できない: {status}（sprites_gen.py の candidate_kind を更新する）")


def candidates():
    """docs/sprite-candidates/<topic>/candidates.txt の全候補を、TOPICS の順・ファイル内の順で返す。"""
    known = {t for t, _ in TOPICS}
    folders = sorted(d for d in os.listdir(CANDIDATES_DIR) if os.path.isdir(os.path.join(CANDIDATES_DIR, d)))
    unknown = [d for d in folders if d not in known]
    if unknown:
        raise ValueError(f"docs/sprite-candidates/{unknown} が TOPICS にない（sprites_gen.py に足す）")

    out = []
    for topic, label in TOPICS:
        path = os.path.join(CANDIDATES_DIR, topic, "candidates.txt")
        lines = open(path, encoding="utf-8").read().split("\n")
        i = 0
        while i < len(lines):
            m = re.match(r"^([A-Z][A-Z0-9_]*) = \($", lines[i])
            if not m:
                i += 1
                continue
            name = m.group(1)
            cm = re.match(r"^# (.+?): (.+) — (.+)$", lines[i - 1]) if i > 0 else None
            if not cm:
                raise ValueError(f"{path}: {name} の直前に「# ID: 特徴 — 状態」のコメントがない")
            short, feature, status = cm.groups()
            j = i + 1
            while lines[j] != ")":
                j += 1
            rows = re.findall(r'"([.#]{24})"', "\n".join(lines[i + 1:j]))
            if len(rows) != H:
                raise ValueError(f"{path}: {name} が {len(rows)} 行（{H} 行が必要）")
            out.append({"topic": topic, "topicLabel": label, "id": name, "short": short, "feature": feature,
                        "status": status, "kind": candidate_kind(status), "rows": rows})
            i = j + 1
    return out


def gen_preview_data():
    """oled_preview.html が読み込む JS（window.MOKOJI_DATA = {...};）を返す。

    フォーム・候補ごとに 1 行にして、差分が追いやすいようにしている。
    """
    dump = lambda o: json.dumps(o, ensure_ascii=False)
    stages = [{"id": i, "label": l} for i, l in STAGES]
    families = [{"id": i, "label": l} for i, l in FAMILIES]
    topics = [{"id": i, "label": l} for i, l in TOPICS]
    lines = [
        "// auto-generated from sprites.py and docs/sprite-candidates — do not edit by hand",
        "// 再生成: python3 sprites_gen.py",
        "window.MOKOJI_DATA = {",
        f' "stages": {dump(stages)},',
        f' "families": {dump(families)},',
        f' "topics": {dump(topics)},',
        ' "forms": [',
        ",\n".join("  " + dump(f) for f in forms()),
        " ],",
        ' "candidates": [',
        ",\n".join("  " + dump(c) for c in candidates()),
        " ]",
        "};",
    ]
    return "\n".join(lines) + "\n"


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
        with open("oled_preview_data.js", "w", encoding="utf-8") as f:
            f.write(gen_preview_data())
        print("sprites.h と oled_preview_data.js を再生成しました")
