#pragma once
#include <cstdint>

constexpr int MAX_GRAVES = 5;

struct GraveRecord {
    char     name[8];
    char     breed[16];
    uint16_t age_days;
};

// フラッシュに書き込むセーブデータ。サイズが変わったら MAGIC を変えて互換性を切る。
struct GameSaveData {
    static constexpr uint32_t MAGIC = 0x4D4F4B31;  // 'MOK1'（lifespan_days 追加で MOK0 から bump）

    uint32_t magic;
    char     name[8];
    uint8_t  stage;          // Game::Stage の生値
    uint8_t  breed;          // Game::Breed の生値
    uint8_t  sheep_type;     // Game::SheepType の生値
    uint8_t  hunger;
    uint8_t  happy;
    uint8_t  sleepy;
    uint8_t  wool;
    uint8_t  sleeping;
    uint8_t  lifespan_days;  // 個体寿命（10-15 日のランダム値）
    uint32_t age_ticks;
    int16_t  tend_feed;
    int16_t  tend_pet;
    int16_t  tend_shear;
    uint8_t  grave_count;
    GraveRecord graves[MAX_GRAVES];
};

// フラッシュ末尾の 4KB セクタにデータを保存する。
class Save {
public:
    Save() = default;
    bool load(GameSaveData* out) const;     // 有効なデータがあれば true
    void write(const GameSaveData& data);   // セクタ消去 → プログラム
};
