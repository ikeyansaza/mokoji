# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## プロジェクト概要

MOKOJI — Raspberry Pi Pico + SSD1306 OLED で動く携帯型の羊育成ゲームデバイス。C++17 / Pico SDK。
携帯版 v1.0 で XIAO RP2350 + PlatformIO に移行予定。

## ビルド・テスト

```bash
# 実機ファーム（.uf2）
cmake -B build -DPICO_SDK_PATH=$HOME/pico-sdk && cmake --build build -j

# 進化検証用（1 game-hour = 3 秒）
cmake -B build_fast -DPICO_SDK_PATH=$HOME/pico-sdk -DDEBUG_FAST=ON && cmake --build build_fast -j

# 書き込み
picotool load -x build/mokoji.uf2

# ホスト側ユニットテスト（Mac で直接実行）
cd test && make test
```

テストは `-DHOST_TEST` で Pico SDK を排除し、`test/stubs/` のスタブでビルドする。
テストフレームワークは使わず、assert マクロで検証する自前方式。

## コミット・開発ルール

- コミットメッセージは日本語
- mainへの直接pushは、E2E修正やtypo修正など軽微な1コミットの場合のみ許可
- テストを書いてからコードを実装する（TDD）
- ライセンス: © 2026 Naofumi Ikeda — All rights reserved（詳細は LICENSE）

## アーキテクチャ

### レイヤー構成

```
main.cpp          ハードウェア初期化・ゲームループ・ボタンポーリング・セーブ制御
  ├── Game        ゲームロジック（進化・ステータス・命名・墓）── Pico SDK 非依存
  ├── JumpGame    ミニゲーム「柵を跳ぶ羊」のロジック（時刻から進む純粋なロジック）── Pico SDK 非依存
  ├── Display     Game の状態を読み取って SSD1306 に描画
  ├── SSD1306     I2C OLED ドライバ最小実装（フレームバッファ → show() で転送）
  ├── Sound       パッシブブザーの PWM 効果音
  ├── Melody      音符列を時刻から進める純粋なロジック（亡くなったときのエンディング曲）── Pico SDK 非依存
  └── Save        フラッシュ末尾 4KB セクタへのバイナリ保存（CRC32 検証付き）
```

Game クラスはハードウェアに一切依存しない。Sound はコンストラクタで注入し、nullptr 許容（テスト時）。
ミニゲームは `Screen::MINIGAME` で `JumpGame` を進め、結果（幸福度・空腹）だけ Game が反映する。ゲーム中の効果音は
待たない `Sound::blip` / `update` を使い（`mee()` などは `sleep_ms` で待つので使わない）、`main.cpp` はゲーム中だけループを速くする。
エンディング曲（`melody::kEnding`。約 29 秒、オリジナル）も待たずに、`Sound::playEnding` で始めて `update` で進める。墓の画面でボタンを押すと止まり、命名へ進む。
曲の間も `main.cpp` はループを速くする。
Display は Game の const 参照だけ受け取る read-only 設計。

### 進化システム（3 系統 × 7 品種）

```
BABY（共通）
  ├─ YOUNG_MOKO    → CORRIEDALE / LINCOLN（MERINO は、一旦、進化先から外している。絵は docs/sprite-candidates/merino/）
  ├─ YOUNG_SUFFOLK → SUFFOLK / HAMPSHIRE
  └─ YOUNG_WILD   → MOUFLON / BIGHORN
```

進化判定は、進化のときの空腹（`hunger`）・幸福（`happy`）の値と乱数で決まる（重みは `Game::youngChoices` /
`adultChoices`。満腹 → モコ系・BIGHORN、幸福 → サフォーク系・HAMPSHIRE、どちらも低い → ワイルド系）。
世話の傾向スコア（`tend_*`）は使わない（セーブの領域だけ残し、0 を書く）。
毛・角は成体だけ伸び、毛刈り・角研ぎ（モコ・サフォーク系: 毛刈り、ワイルド系: 角研ぎ）も成体のメニューにだけ出る。
ベビーと若羊のメニューは 5 項目（ごはん・なでる・ゲーム・プロフ・もどる）。

### セーブ設計

`GameSaveData` 構造体をフラッシュ末尾セクタに直接書き込み。MAGIC (`'MOK5'`) + CRC32 で整合性検証。
Flash 寿命保護: dirty フラグ + 30 秒 rate limit + 30 分 force-save。

### GPIO ピン割り当て（Pico プロトタイプ版）

| 機能 | GPIO |
|------|------|
| SDA  | GP16 |
| SCL  | GP17 |
| BUZZER | GP18 |
| BTN_RIGHT | GP19 |
| BTN_CENTER | GP20 |
| BTN_LEFT | GP21 |

## スプライト

`sprites.h` は `sprites_gen.py` で自動生成。手編集しない。
元素材は `素材/` ディレクトリ（gitignore 済み、dot-illust.net 由来・非商用 OK）。

## 日本語フォント

`kana_font.h`（ひらがな 75 字、名前入力用）と `ja_font.h`（かな・カタカナ・記号・JIS 第一水準漢字、UTF-8 表示用）は
`kana_font_gen.py` で美咲ゴシック（8x8、商用可・再配布自由）の BDF から自動生成。手編集しない。
ライセンス文は `third_party/misaki/misaki.txt`。`SSD1306::drawText` は UTF-8 対応で、ASCII は 5x7、それ以外は 8x8 で描く。
