/*
 FLIF encoder - Free Lossless Image Format
 Copyright (C) 2010-2015  Jon Sneyers & Pieter Wuille, LGPL v3+

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

template <typename RAC>
void UniformSymbolCoder<RAC>::write_int(int min, int max, int val) {
        assert(max >= min);
        if (min != 0) {
            max -= min;
            val -= min;
        }
        if (max == 0) return;

        // split in [0..med] [med+1..max]
        int med = max/2;
        if (val > med) {
            rac.write_bit(true);
            write_int(med+1, max, val);
        } else {
            rac.write_bit(false);
            write_int(0, med, val);
        }
        return;
}


template <typename SymbolCoder> void writer(SymbolCoder& coder, int bits, int value) {
  int pos=0;
  while (pos++ < bits) {
    coder.write(value&1, BIT_MANT, pos);
    value >>= 1;
  }
}

template <int bits, typename SymbolCoder> void writer(SymbolCoder& coder, int min, int max, int value) {
    assert(min<=max);
    assert(value>=min);
    assert(value<=max);

    // avoid doing anything if the value is already known
    if (min == max) return;

    if (value == 0) { // value is zero
        coder.write(true, BIT_ZERO);
        return;
    }

    assert(min <= 0 && max >= 0); // should always be the case, because guess should always be in valid range

    // only output zero bit if value could also have been zero
    //if (max >= 0 && min <= 0) 
    coder.write(false,BIT_ZERO);
    int sign = (value > 0 ? 1 : 0);
    if (max > 0 && min < 0) {
        // only output sign bit if value can be both pos and neg
        coder.write(sign,BIT_SIGN);
    }
    if (sign) min = 1;
    if (!sign) max = -1;
    const int a = abs(value);
    const int e = maniac::util::ilog2(a);
    int amin = sign ? abs(min) : abs(max);
    int amax = sign ? abs(max) : abs(min);

    int emax = maniac::util::ilog2(amax);
    int i = maniac::util::ilog2(amin);

    while (i < emax) {
        // if exponent >i is impossible, we are done
        if ((1 << (i+1)) > amax) break;
        // if exponent i is possible, output the exponent bit
        coder.write(i==e, BIT_EXP, (i<<1) + sign);
        if (i==e) break;
        i++;
    }
//  e_printf("exp=%i\n",e);
    int have = (1 << e);
    int left = have-1;
    for (int pos = e; pos>0;) {
        int bit = 1;
        left ^= (1 << (--pos));
        int minabs1 = have | (1<<pos);
        // int maxabs1 = have | left | (1<<pos);
        // int minabs0 = have;
        int maxabs0 = have | left;
        if (minabs1 > amax) { // 1-bit is impossible
            bit = 0;
        } else if (maxabs0 >= amin) { // 0-bit and 1-bit are both possible
            bit = (a >> pos) & 1;
            coder.write(bit, BIT_MANT, pos);
        }
        have |= (bit << pos);
    }
}


template <typename BitChance, typename RAC, int bits>
void SimpleSymbolBitCoder<BitChance,RAC,bits>::write(bool bit, SymbolChanceBitType typ, int i) {
        BitChance& bch = ctx.bit(typ, i);
        rac.write_12bit_chance(bch.get_12bit(), bit);
        bch.put(bit, table);
}


template <typename BitChance, typename RAC, int bits>
void SimpleSymbolCoder<BitChance,RAC,bits>::write_int(int min, int max, int value) {
        SimpleSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, ctx, rac);
        writer<bits, SimpleSymbolBitCoder<BitChance, RAC, bits> >(bitCoder, min, max, value);
}
template <typename BitChance, typename RAC, int bits>
void SimpleSymbolCoder<BitChance,RAC,bits>::write_int(int nbits, int value) {
        assert (nbits <= bits);
        SimpleSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, ctx, rac);
        writer(bitCoder, nbits, value);
}
