#pragma once

#include <stdint.h>

struct CRC32KTable {
    CRC32KTable();
    uint_fast32_t tab[256];
};

extern const CRC32KTable crc32k;

inline void crc32k_transform(uint_fast32_t& crc, const uint8_t byte) {
  crc = ((crc) << 8) ^ crc32k.tab[(((crc) >> 24) ^ (byte)) & 0xFF];
}
