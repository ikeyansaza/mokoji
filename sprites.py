# ============================================================
# mokomoko - sprites.py
# 24×24 ピクセルスプライト（16 種類、各 _F / _L / _R）
#
# ⚠️ 重要：現在のデザインは dot-illust.net 由来の参考素材ベース。
# 非商用なら利用可だが、IP 展開・商用化時はオリジナルに差し替え必須。
#
# 進化ツリー：
#   BABY (1) → YOUNG (3 系統) → ADULT (6 品種) → SPECIAL (4 増毛 + 2 角長)
#
# 詳細仕様：memory/project_mokoji_evolution_spec.md
# ============================================================


def _r(s):
    """24 文字パターン (#=ON, .=OFF) を [b0, b1, b2] に変換"""
    b = [0, 0, 0]
    for i, c in enumerate(s[:24]):
        if c == '#':
            b[i >> 3] |= 1 << (7 - (i & 7))
    return b


def _img(*rows):
    return [_r(r) for r in rows]


def _flip(sp):
    """スプライトを左右反転（L→R 自動生成用）"""
    out = []
    for row in sp:
        v = (row[0] << 16) | (row[1] << 8) | row[2]
        rev = 0
        for i in range(24):
            if (v >> i) & 1:
                rev |= 1 << (23 - i)
        out.append([(rev >> 16) & 0xFF, (rev >> 8) & 0xFF, rev & 0xFF])
    return out


# ============================================================
# 共通の羊シルエット（hitsuji.svg ベース、19x16 → 24x24 center pad）
# ★ オリジナル絵に差し替えるまでの placeholder ★
# 16 種類すべてこの形をベースに、ユーザーが順次描き直して差し替える。
# ============================================================
HITSUJI = (
    "........................",
    "........................",
    "........................",
    "........................",
    "..........#.#...........",
    "........#######.........",
    "......###########.......",
    ".....####...#.####......",
    "......#..#####..#.......",
    "...###.#########.###....",
    "......###########.......",
    "....#.#.#######.#.#.....",
    "...##.###########.##....",
    "....#.###########.#.....",
    "....##.#########.##.....",
    "....##.###.#.###.##.....",
    ".....##.###.###.##......",
    ".....###.......###......",
    "......###########.......",
    "........##..#.#.........",
    "........................",
    "........................",
    "........................",
    "........................",
)


def _placeholder():
    """オリジナル差し替え前の暫定スプライト。"""
    return _img(*HITSUJI)


# ============================================================
# Stage 1: ベビーシープ（共通子供、1 種）
# ============================================================
BABY_F = _placeholder()
BABY_L = _placeholder()
BABY_R = _flip(BABY_L)


# ============================================================
# Stage 2: 若羊期（3 系統）
# ============================================================

# モコ系若羊
YOUNG_MOKO_F = _placeholder()
YOUNG_MOKO_L = _placeholder()
YOUNG_MOKO_R = _flip(YOUNG_MOKO_L)

# サフォーク系若羊
YOUNG_SUFFOLK_F = _placeholder()
YOUNG_SUFFOLK_L = _placeholder()
YOUNG_SUFFOLK_R = _flip(YOUNG_SUFFOLK_L)

# ワイルド系若羊
YOUNG_WILD_F = _placeholder()
YOUNG_WILD_L = _placeholder()
YOUNG_WILD_R = _flip(YOUNG_WILD_L)


# ============================================================
# Stage 3: 成体（6 品種）
# ============================================================

# モコ系
ADULT_MERINO_F = _placeholder()
ADULT_MERINO_L = _placeholder()
ADULT_MERINO_R = _flip(ADULT_MERINO_L)

ADULT_CORRIEDALE_F = _placeholder()
ADULT_CORRIEDALE_L = _placeholder()
ADULT_CORRIEDALE_R = _flip(ADULT_CORRIEDALE_L)

# サフォーク系
ADULT_SUFFOLK_F = _placeholder()
ADULT_SUFFOLK_L = _placeholder()
ADULT_SUFFOLK_R = _flip(ADULT_SUFFOLK_L)

ADULT_HAMPSHIRE_F = _placeholder()
ADULT_HAMPSHIRE_L = _placeholder()
ADULT_HAMPSHIRE_R = _flip(ADULT_HAMPSHIRE_L)

# ワイルド系
ADULT_MOUFLON_F = _placeholder()
ADULT_MOUFLON_L = _placeholder()
ADULT_MOUFLON_R = _flip(ADULT_MOUFLON_L)

ADULT_BIGHORN_F = _placeholder()
ADULT_BIGHORN_L = _placeholder()
ADULT_BIGHORN_R = _flip(ADULT_BIGHORN_L)


# ============================================================
# Stage 4: 特殊成長
# ============================================================

# 増毛期（モコ系・サフォーク系の 4 品種、毛が伸びた状態）
ADULT_MERINO_FLUFFY_F = _placeholder()
ADULT_MERINO_FLUFFY_L = _placeholder()
ADULT_MERINO_FLUFFY_R = _flip(ADULT_MERINO_FLUFFY_L)

ADULT_CORRIEDALE_FLUFFY_F = _placeholder()
ADULT_CORRIEDALE_FLUFFY_L = _placeholder()
ADULT_CORRIEDALE_FLUFFY_R = _flip(ADULT_CORRIEDALE_FLUFFY_L)

ADULT_SUFFOLK_FLUFFY_F = _placeholder()
ADULT_SUFFOLK_FLUFFY_L = _placeholder()
ADULT_SUFFOLK_FLUFFY_R = _flip(ADULT_SUFFOLK_FLUFFY_L)

ADULT_HAMPSHIRE_FLUFFY_F = _placeholder()
ADULT_HAMPSHIRE_FLUFFY_L = _placeholder()
ADULT_HAMPSHIRE_FLUFFY_R = _flip(ADULT_HAMPSHIRE_FLUFFY_L)

# 角長期（ワイルド系の 2 品種、角が伸びた状態）
ADULT_MOUFLON_LONGHORN_F = _placeholder()
ADULT_MOUFLON_LONGHORN_L = _placeholder()
ADULT_MOUFLON_LONGHORN_R = _flip(ADULT_MOUFLON_LONGHORN_L)

ADULT_BIGHORN_LONGHORN_F = _placeholder()
ADULT_BIGHORN_LONGHORN_L = _placeholder()
ADULT_BIGHORN_LONGHORN_R = _flip(ADULT_BIGHORN_LONGHORN_L)


# ============================================================
# 旧名エイリアス（game.cpp / display.cpp の段階的移行用、後で消す）
# ============================================================
LAMB_F = BABY_F
LAMB_L = BABY_L
LAMB_R = BABY_R

# YOUNG_MOKO_* は名前そのまま（新仕様でも同名）
YOUNG_SURA_F = YOUNG_SUFFOLK_F
YOUNG_SURA_L = YOUNG_SUFFOLK_L
YOUNG_SURA_R = YOUNG_SUFFOLK_R
YOUNG_RARE_F = YOUNG_WILD_F
YOUNG_RARE_L = YOUNG_WILD_L
YOUNG_RARE_R = YOUNG_WILD_R

ADULT_COR_F = ADULT_CORRIEDALE_F
ADULT_COR_L = ADULT_CORRIEDALE_L
ADULT_COR_R = ADULT_CORRIEDALE_R
ADULT_MER_F = ADULT_MERINO_F
ADULT_MER_L = ADULT_MERINO_L
ADULT_MER_R = ADULT_MERINO_R
ADULT_SUF_F = ADULT_SUFFOLK_F
ADULT_SUF_L = ADULT_SUFFOLK_L
ADULT_SUF_R = ADULT_SUFFOLK_R
# 旧 SOUTHDOWN は HAMPSHIRE に統合（同系統の暗顔系）
ADULT_SOU_F = ADULT_HAMPSHIRE_F
ADULT_SOU_L = ADULT_HAMPSHIRE_L
ADULT_SOU_R = ADULT_HAMPSHIRE_R
# 旧 EASTFRIESIAN は MOUFLON に仮マッピング（次フェーズで再設計）
ADULT_EAST_F = ADULT_MOUFLON_F
ADULT_EAST_L = ADULT_MOUFLON_L
ADULT_EAST_R = ADULT_MOUFLON_R


# ロード後はヘルパーは不要（RAM を節約）
del _r, _img, _flip, _placeholder
