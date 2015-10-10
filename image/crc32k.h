#pragma once

#include <stdint.h>

struct CRC32KTable {
    CRC32KTable();
    uint_fast32_t tab[256];
};

extern const CRC32KTable crc32k;

#define crc32k_transform(var,byte) do { (var) = ((var) << 8) ^ crc32k.tab[(((var) >> 24) ^ (byte)) & 0xFF]; } while(0);
