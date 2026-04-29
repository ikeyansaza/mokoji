# MOKOJI — C++ port

Raspberry Pi Pico (RP2040) 向け C++ 実装。MicroPython 版（リポジトリ root）の機能ポート。

## 必要なもの

- Raspberry Pi Pico SDK（`PICO_SDK_PATH` 環境変数または `-DPICO_SDK_PATH=...` で指定）
- ARM GNU Toolchain（`arm-none-eabi-gcc` 系。Homebrew なら `brew install --cask gcc-arm-embedded`）
- CMake 3.13 以上
- picotool（フラッシュ書き込み用、`brew install picotool`）

## ビルド

```bash
cd cpp
cmake -B build -DPICO_SDK_PATH=$HOME/pico-sdk
cmake --build build -j
```

成果物は `build/mokoji.uf2`。Pico を BOOTSEL ボタン押しながら接続して、UF2 を D&D で書き込み。

または picotool 経由：

```bash
picotool load -x build/mokoji.uf2
```

## ハードウェア接続

| パーツ              | ピン                               |
|---------------------|------------------------------------|
| OLED (SSD1306, I2C) | SDA=GP16, SCL=GP17, 0x3C, 400kHz   |
| ボタン左            | GP10 (内部プルアップ、active LOW)  |
| ボタン中央          | GP11                               |
| ボタン右            | GP12                               |
| パッシブブザー      | GP15 (PWM)                         |

## モジュール

| ファイル          | 役割                                      |
|-------------------|-------------------------------------------|
| `main.cpp`        | エントリ・ハードウェア初期化・メインループ |
| `game.{h,cpp}`    | ゲームロジック・進化・睡眠・墓             |
| `display.{h,cpp}` | OLED 描画                                 |
| `sound.{h,cpp}`   | PWM 効果音                                |
| `save.{h,cpp}`    | フラッシュ末尾セクタへセーブ               |
| `ssd1306.{h,cpp}` | OLED ドライバ最小実装                      |
| `sprites.h`       | 27 枚のスプライトデータ（auto-generated）  |
| `font.{h,cpp}`    | 5×7 ASCII フォント                        |
