# MOKOJI 🐑

> モコモコな羊の略。めん羊育成デバイス。

Raspberry Pi Pico + OLED で動く、手のひらサイズの羊育成ゲーム。
進化する瞬間の空腹・幸福度と乱数によって、毎回違う品種の羊に育つ。

C++17（Pico SDK）で実装。プロトタイプ段階、実機で動作確認済み。

---

## コンセプト

- 「同じ育て方でも毎回違う子になる」体験を重視
- 動物はコントロール可能な奇跡の存在、というテーマ
- 画面の中だけでなく、手元で鳴り光る「物理・手触りのあるモノ」をつくりたかった
- 将来的には動物図鑑・環世界エクスプローラーと連携する三層構造の一部

---

## できること

- 世話：ごはん／なでる／毛刈り（モコ・サフォーク系）／角研ぎ（ワイルド系）
- ミニゲーム「柵を跳ぶ羊」
- 名前入力（プリセット 20 種、または、ひらがな手入力）
- 就寝サイクル、死亡・お墓・エンディング曲
- 検証用ビルドでは、メイン画面で LEFT 長押しにより、姿を進化の順に 1 つずつ進められる

---

## ハードウェア（プロトタイプ版）

| パーツ            | ピン                        | 備考                       |
|-------------------|-----------------------------|----------------------------|
| Raspberry Pi Pico | —                           | RP2040                     |
| OLED 0.96" 128×64 | SDA=GP16 / SCL=GP17 / 0x3C  | SSD1306, I2C 400kHz        |
| ボタン左          | GP21                        | 内部プルアップ・active LOW |
| ボタン中央        | GP20                        | 同上                       |
| ボタン右          | GP19                        | 同上                       |
| パッシブブザー    | GP18                        | PWM 制御                   |

Pico 本体が左側ピン（GP0-15）の一部をブレッドボード上で覆ってしまうため、ボタン・ブザーは右側ピンに割り当てている。
携帯版 v1.0 は Seeed XIAO RP2350 + PlatformIO への移行を予定（ピン配置は別途）。外装はフェルトで手作り予定。

---

## 進化システム

```
BABY（共通の子羊）
    ↓ 進化する瞬間の空腹・幸福値 × 乱数
YOUNG_MOKO / YOUNG_SUFFOLK / YOUNG_WILD（3 系統の若羊）
    ↓ 同上
ADULT（3 系統 × 7 品種）
  ├── モコ系      → コリデール / リンカーン
  ├── サフォーク系 → サフォーク / ハンプシャー
  └── ワイルド系   → ムフロン / ビッグホーン
```

- お世話の「傾向」を累積して見るのではなく、進化する瞬間の空腹・幸福バーの値だけで重みが決まる
- 成体は毛（モコ・サフォーク系）・角（ワイルド系）が伸び、毛刈り／角研ぎで元の姿に戻す
- 進化先はプロフィール画面で確認できる

---

## ビルド

### 必要なもの

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)（`PICO_SDK_PATH` 環境変数で指定）
- ARM GNU Toolchain（`brew install --cask gcc-arm-embedded` 推奨）
- CMake 3.13 以上
- picotool（`brew install picotool`）

### コマンド

```bash
cmake -B build -DPICO_SDK_PATH=$HOME/pico-sdk
cmake --build build -j
```

成果物は `build/mokoji.uf2`。Pico を BOOTSEL 押しながら接続して D&D で書き込み、または：

```bash
picotool load -x build/mokoji.uf2
```

### 進化検証用の高速モード

ゲーム内 1 時間を 3 秒に短縮するビルドオプション：

```bash
cmake -B build_fast -DPICO_SDK_PATH=$HOME/pico-sdk -DDEBUG_FAST=ON
cmake --build build_fast -j
```

このビルドでは、メイン画面で LEFT 長押しをすると、姿を進化の順（ベビー → 各系統の若羊 → 成体）に 1 つずつ切り替えられる。

---

## 開発用プレビューツール

`oled_preview.html` をブラウザで開くと、実機なしでスプライト・進化ツリー（16 種）・アニメーションを確認できる。
`sprites_gen.py` が出力する `oled_preview_data.js` を読み込む仕組みで、スプライト候補の検討や、進化ツリーの実装確認に使っている。

---

## ホスト側ユニットテスト

`game.cpp` 以下、ハード非依存なロジック（進化・ステータス減衰・save/load・ミニゲーム・命名 UI 等）は
Mac で直接検証できる。

```bash
cd test
make test
```

134 ケース、毎回 1 秒未満で完走する。

---

## ファイル構成

```
mokoji/
├── CMakeLists.txt             # Pico SDK ビルド設定
├── pico_sdk_import.cmake      # SDK 導入用ボイラープレート
├── main.cpp                   # エントリ・ハードウェア初期化・ゲームループ
├── game.{h,cpp}                # ゲームロジック（進化・ステータス・命名・墓）。Pico SDK 非依存
├── jump_game.{h,cpp}           # ミニゲーム「柵を跳ぶ羊」のロジック。Pico SDK 非依存
├── display.{h,cpp}             # Game の状態を読み取って SSD1306 に描画
├── ssd1306.{h,cpp}             # SSD1306 OLED ドライバ最小実装
├── background.{h,cpp}          # 背景（草・空・布団）の描画
├── eat_motion.{h,cpp}           # 各アクションの動き（Pico SDK 非依存の純粋関数）。
├── pet_motion.{h,cpp}           # Display が見て描き、Sound（Game 経由）が見て鳴らす
├── shear_motion.{h,cpp}         #   ごはん／なでる／毛刈り／角研ぎ
├── polish_motion.{h,cpp}        #
├── melody.{h,cpp}              # エンディング曲を時刻から進めるロジック。Pico SDK 非依存
├── sound.{h,cpp}                # パッシブブザーの PWM 効果音
├── save.{h,cpp}                 # フラッシュ末尾セクタへのバイナリ保存（CRC32 検証付き）
├── font.{h,cpp}                 # ASCII 5x7 フォント
├── kana.{h,cpp}                 # ひらがな名前入力の状態管理
├── kana_font.h / ja_font.h     # 日本語フォント（美咲ゴシック、kana_font_gen.py で生成）
├── sprites.h                    # スプライトデータ（sprites_gen.py で自動生成、手編集しない）
├── sprites.py / sprites_gen.py / kana_font_gen.py   # スプライト・フォント生成スクリプト
├── oled_preview.html / oled_preview_data.js         # ブラウザ用スプライト・進化ツリープレビュー
├── docs/                        # 進化仕様・スプライト候補の検討メモ
└── test/                        # ホスト側ユニットテスト
    ├── Makefile
    ├── test_game.cpp
    └── stubs/                   # Pico SDK 非依存にする stub 群
```

---

## ロードマップ

| フェーズ | 内容                                                        |
|----------|---------------------------------------------------------------|
| v0.1-v0.4 | 済み：ハード組み立て・ゲームロジック・効果音・ミニゲーム・進化・お墓 |
| v1.0     | 携帯型化：Seeed XIAO RP2350 + PlatformIO 移行、フェルト外装完成      |
| v2.0     | 電池ボックス変更                                              |
| v3.0     | 動物図鑑・環世界エクスプローラー連携                              |

---

## License

© 2026 Naofumi Ikeda — All rights reserved（詳細は LICENSE）
