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

#pragma once

#include <vector>
#include <assert.h>
#include "util.hpp"
#include "chance.hpp"
#include "../compiler-specific.hpp"

template <typename RAC> class UniformSymbolCoder {
private:
    RAC &rac;

public:
    explicit UniformSymbolCoder(RAC &racIn) : rac(racIn) { }

#ifdef HAS_ENCODER
    void write_int(int min, int max, int val);
    void write_int(int bits, int val) { write_int(0, (1<<bits) -1, val); };
#endif
    int read_int(int min, int len) {
        assert(len >= 0);
        if (len == 0) return min;

        // split in [0..med] [med+1..len]
        int med = len/2;
        bool bit = rac.read_bit();
        if (bit) {
            return read_int(min+med+1, len-(med+1));
        } else {
            return read_int(min, med);
        }
    }
    int read_int(int bits) { return read_int(0, (1<<bits)-1); }
};

typedef enum {
    BIT_ZERO,
    BIT_SIGN,

    BIT_EXP,
    BIT_MANT,
//    BIT_EXTRA
} SymbolChanceBitType;

//static const char *SymbolChanceBitName[] = {"zero", "sign", "expo", "mant"};

static const uint16_t EXP_CHANCES[] = {1000, 1200, 1500, 1750, 2000, 2300, 2800, 2400, 2300,
                                       2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048};
static const uint16_t MANT_CHANCES[] = {1900, 1850, 1800, 1750, 1650, 1600, 1600, 2048, 2048,
                                        2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048};
static const uint16_t ZERO_CHANCE = 1000;
static const uint16_t SIGN_CHANCE = 2048;
/*
static const uint16_t EXP_CHANCES[] = {1200, 1600, 1800, 1900, 2050, 2300, 2500, 2300, 2048,
                                       2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048};
static const uint16_t MANT_CHANCES[] = {1750, 1730, 1710, 1670, 1650, 1700, 1800, 1800, 2048,
                                        2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048};
static const uint16_t ZERO_CHANCE = 1600;
static const uint16_t SIGN_CHANCE = 2048;
*/
#ifdef STATS
struct SymbolChanceStats {
    BitChanceStats stats_zero;
    BitChanceStats stats_sign;
    BitChanceStats stats_exp[17];
    BitChanceStats stats_mant[18];

    std::string format() const;
    ~SymbolChanceStats();
};

extern SymbolChanceStats global_symbol_stats;
#endif

template <typename BitChance, int bits> class SymbolChance {
    BitChance bit_zero;
    BitChance bit_sign;
    BitChance bit_exp[(bits-1) * 2];
    BitChance bit_mant[bits];

public:

    BitChance inline &bitZero()      {
        return bit_zero;
    }

    BitChance inline &bitSign()      {
        return bit_sign;
    }

    // all exp bits 0         -> int(log2(val)) == 0  [ val == 1 ]
    // exp bits up to i are 1 -> int(log2(val)) == i+1
    BitChance inline &bitExp(int i)  {
        assert(i >= 0 && i < 2*(bits-1));
        return bit_exp[i];
    }
    BitChance inline &bitMant(int i) {
        assert(i >= 0 && i < bits);
        return bit_mant[i];
    }

    BitChance inline &bit(SymbolChanceBitType typ, int i = 0) {
        switch (typ) {
        default:
        case BIT_ZERO:
            return bitZero();
        case BIT_SIGN:
            return bitSign();
        case BIT_EXP:
            return bitExp(i);
        case BIT_MANT:
            return bitMant(i);
        }
    }
    SymbolChance() { // : bit_exp(bitsin-1), bit_mant(bitsin) {
        bitZero().set_12bit(ZERO_CHANCE);
        bitSign().set_12bit(SIGN_CHANCE);
//        printf("bits: %i\n",bits);
        for (int i=0; i<bits-1; i++) {
            bitExp(2*i).set_12bit(EXP_CHANCES[i]);
            bitExp(2*i+1).set_12bit(EXP_CHANCES[i]);
        }
        for (int i=0; i<bits; i++) {
            bitMant(i).set_12bit(MANT_CHANCES[i]);
        }
    }

    int scale() const {
        return bitZero().scale();
    }

#ifdef STATS
    ~SymbolChance() {
        global_symbol_stats.stats_zero += bit_zero.stats();
        global_symbol_stats.stats_sign += bit_sign.stats();
        for (int i = 0; i < bits - 1 && i < 17; i++) {
            global_symbol_stats.stats_exp[i] += bit_exp[i].stats();
        }
        for (int i = 0; i < bits && i < 18; i++) {
            global_symbol_stats.stats_mant[i] += bit_mant[i].stats();
        }
    }
#endif
};

template <typename SymbolCoder> int reader(SymbolCoder& coder, int bits) {
  int pos=0;
  int value=0;
  int b=1;
  while (pos++ < bits) {
    if (coder.read(BIT_MANT, pos)) value += b;
    b *= 2;
  }
  return value;
}

template <int bits, typename SymbolCoder> int reader(SymbolCoder& coder, int min, int max) ATTRIBUTE_HOT;

template <int bits, typename SymbolCoder> int reader(SymbolCoder& coder, int min, int max) {
    assert(min<=max);
    if (min == max) return min;

    bool sign;
    assert(min <= 0 && max >= 0); // should always be the case, because guess should always be in valid range

      if (coder.read(BIT_ZERO)) return 0;
      if (min < 0) {
        if (max > 0) {
          sign = coder.read(BIT_SIGN);
        } else {sign = false; }
      } else {sign = true; }

    const int amin = 1;
    const int amax = (sign? max : -min);

    const int emax = maniac::util::ilog2(amax);
    int e = maniac::util::ilog2(amin);

    for (; e < emax; e++) {
        // if exponent >e is impossible, we are done
        // actually that cannot happen
        //if ((1 << (e+1)) > amax) break;
        if (coder.read(BIT_EXP,(e<<1)+sign)) break;
    }

    int have = (1 << e);
    int left = have-1;
    for (int pos = e; pos>0;) {
        //int bit = 1;
        //left ^= (1 << (--pos));
        left >>= 1; pos--;
        int minabs1 = have | (1<<pos);
        int maxabs0 = have | left;
        if (minabs1 > amax) { // 1-bit is impossible
            //bit = 0;
            continue;
        } else if (maxabs0 >= amin) { // 0-bit and 1-bit are both possible
            //bit = coder.read(BIT_MANT,pos);
            if (coder.read(BIT_MANT,pos)) have = minabs1;
        } // else 0-bit is impossible, so bit stays 1
        else have = minabs1;
        //have |= (bit << pos);
    }
    return (sign ? have : -have);
}

template <typename BitChance, typename RAC, int bits> class SimpleSymbolBitCoder {
    typedef typename BitChance::Table Table;

private:
    const Table &table;
    SymbolChance<BitChance,bits> &ctx;
    RAC &rac;

public:
    SimpleSymbolBitCoder(const Table &tableIn, SymbolChance<BitChance,bits> &ctxIn, RAC &racIn) : table(tableIn), ctx(ctxIn), rac(racIn) {}

#ifdef HAS_ENCODER
    void write(bool bit, SymbolChanceBitType typ, int i = 0);
#endif

    bool read(SymbolChanceBitType typ, int i = 0) {
        BitChance& bch = ctx.bit(typ, i);
        bool bit = rac.read_12bit_chance(bch.get_12bit());
        bch.put(bit, table);
        return bit;
    }
};

template <typename BitChance, typename RAC, int bits> class SimpleSymbolCoder {
    typedef typename BitChance::Table Table;

private:
    SymbolChance<BitChance,bits> ctx;
    const Table table;
    RAC &rac;

public:
    SimpleSymbolCoder(RAC& racIn, int cut = 2, int alpha = 0xFFFFFFFF / 19) :  table(cut,alpha), rac(racIn) {
    }

#ifdef HAS_ENCODER
    void write_int(int min, int max, int value);
    void write_int2(int min, int max, int value) {
        ////int avg = (min+max)/2;
        //int avg = min;
        //write_int(min-avg,max-avg,value-avg);
        if (min>0) write_int(0,max-min,value-min);
        else if (max<0) write_int(min-max,0,value-max);
        else write_int(min,max,value);
    }
    void write_int(int nbits, int value);
#endif

    int read_int(int min, int max) {
        SimpleSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, ctx, rac);
        return reader<bits, SimpleSymbolBitCoder<BitChance, RAC, bits>>(bitCoder, min, max);
    }
    int read_int2(int min, int max) {
        ////int avg = (min+max)/2;
        //int avg = min;
        //return read_int(min-avg,max-avg)+avg;
        if (min > 0) return read_int(0,max-min)+min;
        else if (max<0) return read_int(min-max,0)+max;
        else return read_int(min,max);
    }
    int read_int(int nbits) {
        assert (nbits <= bits);
        SimpleSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, ctx, rac);
        return reader(bitCoder, nbits);
    }
};

#ifdef HAS_ENCODER
#include "symbol_enc.hpp"
#endif
