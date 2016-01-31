#include "../precompile.h"
#include "crc32k.hpp"

CRC32KTable::CRC32KTable() {
    uint32_t x[8];
    x[0] = 0x741b8cd7;
    for (int i = 1; i < 8; i++) {
        x[i] = (x[i - 1] << 1) ^ ((x[i - 1] >> 31) * x[0]);
    }
    for (int j = 0; j < 256; j++) {
        tab[j] = 0;
        for (int i = 0; i < 8; i++) {
            if ((j >> i) & 1) {
                tab[j] ^= x[i];
            }
        }
    }
}

const CRC32KTable crc32k;
