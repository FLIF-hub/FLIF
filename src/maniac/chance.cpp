/*
FLIF - Free Lossless Image Format

Copyright 2010-2016, Jon Sneyers & Pieter Wuille

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "chance.hpp"
#include "bit.hpp"
#include <string.h>

void build_table(uint16_t *zero_state, uint16_t *one_state, size_t size, uint32_t factor, unsigned int max_p)
{
    const int64_t one = 1LL << 32;
    int64_t p;
    unsigned int last_p8, p8;
    unsigned int i;

    memset(zero_state,0,sizeof(uint16_t) * size);
    memset(one_state,0,sizeof(uint16_t) * size);

    last_p8 = 0;
    p = one / 2;
    for (i = 0; i < size / 2; i++) {
        p8 = (size * p + one / 2) >> 32; //FIXME try without the one
        if (p8 <= last_p8) p8 = last_p8 + 1;
        if (last_p8 && last_p8 < size && p8 <= max_p) one_state[last_p8] = p8;

        p += ((one - p) * factor + one / 2) >> 32;
        last_p8 = p8;
    }

    for (i = size - max_p; i <= max_p; i++) {
        if (one_state[i]) continue;

        p = (i * one + size / 2) / size;
        p += ((one - p) * factor + one / 2) >> 32;
        p8 = (size * p + one / 2) >> 32; //FIXME try without the one
        if (p8 <= i) p8 = i + 1;
        if (p8 > max_p) p8 = max_p;
        one_state[i] = p8;
    }

    for (i = 1; i < size; i++)
        zero_state[i] = size - one_state[size - i];
}

/** Computes an approximation of log(4096 / x) / log(2) * base */
static uint32_t log4kf(int x, uint32_t base) {
    int bits = 8 * sizeof(int) - __builtin_clz(x);
    uint64_t y = ((uint64_t)x) << (32 - bits);
    uint32_t res = base * (13 - bits);
    uint32_t add = base;
    while ((add > 1) && ((y & 0x7FFFFFFF) != 0)) {
        y = (((uint64_t)y) * y + 0x40000000) >> 31;
        add >>= 1;
        if ((y >> 32) != 0) {
            res -= add;
            y >>= 1;
        }
    }
    return res;
}

Log4kTable::Log4kTable() {
    data[0] = 0;
    for (int i = 1; i <= 4096; i++) {
        data[i] = (log4kf(i, (65535UL << 16) / 12) + (1 << 15)) >> 16;
    }
    scale = 65535 / 12;
}

const Log4kTable log4k;
