#include "save.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <cstddef>
#include <cstring>

namespace {
// Pico の 2MB フラッシュ末尾の 1 セクタ（4KB）をセーブ領域に予約。
// コードは先頭から積まれるので、サイズが収まる限り衝突しない。
constexpr uint32_t SAVE_OFFSET = (2u * 1024u * 1024u) - FLASH_SECTOR_SIZE;

// CRC32 (IEEE 802.3 多項式 0xEDB88320)。1.3KB 程度なら bitwise 計算でも十分。
uint32_t crc32(const uint8_t* data, size_t n) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1u));
        }
    }
    return ~crc;
}
}  // namespace

bool Save::load(GameSaveData* out) const {
    auto* p = reinterpret_cast<const GameSaveData*>(XIP_BASE + SAVE_OFFSET);
    if (p->magic != GameSaveData::MAGIC) {
        return false;
    }
    // CRC 検証：crc フィールド以外の全バイトについて計算し、一致すれば valid
    constexpr size_t crc_offset = offsetof(GameSaveData, crc);
    uint32_t expected = crc32(reinterpret_cast<const uint8_t*>(p), crc_offset);
    if (expected != p->crc) {
        return false;   // 部分書き込み等で破損 → セーブ無しと同等扱い
    }
    *out = *p;
    return true;
}

void Save::write(const GameSaveData& data) {
    // セーブ領域は 1 sector (4KB)。データはその範囲に収まる限り何 page でも書ける。
    static_assert(sizeof(GameSaveData) <= FLASH_SECTOR_SIZE,
                  "GameSaveData が 1 sector (4KB) を超えた。SAVE 領域を増やす必要がある");

    static uint8_t buf[FLASH_SECTOR_SIZE];
    memset(buf, 0xFF, sizeof(buf));
    memcpy(buf, &data, sizeof(data));

    // crc を末尾に書き込む（load 側と同じ計算）
    constexpr size_t crc_offset = offsetof(GameSaveData, crc);
    uint32_t crc = crc32(buf, crc_offset);
    auto* dst = reinterpret_cast<GameSaveData*>(buf);
    dst->crc = crc;

    // データサイズを FLASH_PAGE_SIZE (256B) の倍数に切り上げて書き込み
    constexpr uint32_t prog_size =
        ((sizeof(GameSaveData) + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE;

    // フラッシュへの erase/program 中は割り込みを止める（XIP がブロックされるため）
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(SAVE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(SAVE_OFFSET, buf, prog_size);
    restore_interrupts(ints);
}
