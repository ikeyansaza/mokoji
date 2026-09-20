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
  └── Save        フラッシュ末尾 4KB セクタへのバイナリ保存（CRC32 検証付き）
```

Game クラスはハードウェアに一切依存しない。Sound はコンストラクタで注入し、nullptr 許容（テスト時）。
ミニゲームは `Screen::MINIGAME` で `JumpGame` を進め、結果（幸福度・空腹）だけ Game が反映する。ゲーム中の効果音は
待たない `Sound::blip` / `update` を使い（`mee()` などは `sleep_ms` で待つので使わない）、`main.cpp` はゲーム中だけループを速くする。
Display は Game の const 参照だけ受け取る read-only 設計。

### 進化システム（3 系統 × 7 品種）

```
BABY（共通）
  ├─ YOUNG_MOKO    → MERINO / CORRIEDALE / LINCOLN
  ├─ YOUNG_SUFFOLK → SUFFOLK / HAMPSHIRE
  └─ YOUNG_WILD   → MOUFLON / BIGHORN
```

進化判定は `tend_feed` / `tend_pet` / `tend_shear` / `tend_polish` の傾向スコアで決まる。
スコアは game-hour ごとに ×0.97 で減衰し、直近の世話が効く設計。
系統別にメニュー項目が切り替わる（モコ・サフォーク系: CUT、ワイルド系: POLI）。

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
