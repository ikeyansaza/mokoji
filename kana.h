#pragma once
#include <cstdint>

// ひらがな入力・表示のためのテーブル群。
// 各ひらがなを 0..74 の uint8_t index で表現し、END (=0xFF) を終端マーカに使う。
//
// 注意：本ファイルは「内部表現」だけを定義する。グリフ（実際のひらがな bitmap）は
// 未実装で、表示は当面 romaji を 5x7 ASCII フォントで描画する。フォント完成後に
// Display 側だけ差し替えれば呼び出し側は変更不要。
namespace kana {

constexpr uint8_t COUNT        = 75;     // ひらがな総数
constexpr uint8_t END          = 0xFF;   // 終端 / 空きスロット
constexpr int     MAX_NAME     = 4;      // 名前は最大 4 文字
constexpr int     NAME_BUF_LEN = 5;      // 4 文字 + END 終端
constexpr int     PRESET_COUNT = 20;
constexpr int     ROW_COUNT    = 16;

// 各ひらがなのメタ情報（romaji 表現を保持）
struct Info {
    const char* romaji;   // 1-3 ASCII 文字、末尾は '\0'
};
extern const Info TABLE[COUNT];

// 入力 UI の「行」構造。たとえば「あ行」= a/i/u/e/o = index 0..4。
struct Row {
    const char* label;    // UI で表示する短いラベル（UTF-8。"あ行"、"か行" 等）
    uint8_t     start;    // この行の最初の index
    uint8_t     length;   // この行の文字数（3 or 4 or 5）
};
extern const Row ROWS[ROW_COUNT];

// プリセット名（20 個）。各エントリは長さ NAME_BUF_LEN の index 配列で、
// 4 文字未満の場合は END で終端。
extern const uint8_t PRESETS[PRESET_COUNT][NAME_BUF_LEN];

// ---- ヘルパ -----------------------------------------------------------

// kana index 列を ASCII romaji 文字列に変換して out に書き込む。
// 終端 NUL を含めて out_size を超えない。
// 戻り値: 書き込んだ文字数（NUL 含まず）。
int toRomaji(const uint8_t* name_kana, char* out, int out_size);

}  // namespace kana
