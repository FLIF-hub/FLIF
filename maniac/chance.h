#pragma once

#include <vector>
#include <math.h>
#include <stdint.h>

#ifdef STATS
#include <stdlib.h>
#include <stdio.h>
#endif

struct Log4kTable {
    uint16_t data[4097];
    int scale;

    Log4kTable();
};

extern const Log4kTable log4k;

class StaticBitChanceTable
{
public:
    StaticBitChanceTable() {};
};

class StaticBitChance
{
protected:
    uint16_t chance; // stored as a 16-bit number

public:
    typedef StaticBitChanceTable Table;

    StaticBitChance(int chanceIn = 0x800) {
        chance = chanceIn;
    }

    uint16_t inline get() const {
        return chance * 16; // return 16-bit number
    }

    void set(uint16_t chanceIn) {
        chance = chanceIn;
    }

    void inline put(bool, const Table &) {}

    void estim(bool bit, uint64_t &total) const {
        total += log4k.data[bit ? chance : 4096-chance];
    }

    int scale() const {
        return log4k.scale;
    }
};

void extern build_table(uint16_t *zero_state, uint16_t *one_state, size_t size, int factor, unsigned int max_p);

class SimpleBitChanceTable
{
public:
    uint16_t next[2][4096];

    void init(int cut, int alpha) {
        build_table(next[0], next[1], 4096, alpha, 4096-cut);
    }

    SimpleBitChanceTable(int cut = 2, int alpha = 0xFFFFFFFF / 19) {
        init(cut, alpha);
    }
};

class SimpleBitChance
{
protected:
    uint16_t chance; // stored as a 12-bit number

public:
    typedef SimpleBitChanceTable Table;

    SimpleBitChance(int chanceIn = 0x800) {
        chance = chanceIn;
    }

    uint16_t inline get() const {
        return chance*16; // return 16-bit number
    }
    void set(uint16_t chance) {
        this->chance = chance;
    }
    void inline put(bool bit, const Table &table) {
        chance = table.next[bit][chance];
    }

    void estim(bool bit, uint64_t &total) const {
        total += log4k.data[bit ? chance : 4096-chance];
    }

    int scale() const {
        return log4k.scale;
    }

#ifdef STATS
    void dist(std::vector<double> &dist) const {}

    void info_bitchance() const {
        printf("\n");
    }
#endif
};


static const uint32_t MULTISCALE_ALPHAS[] = {
                                                     21590903, 66728412, 214748365, 7413105, 106514140, 10478104,
                                                     //41729744, 5081644, 154536134, 53610361, 14808157, 83024800,
                                                     //31468986, 3483142, 8675949, 6333940,128332124,12654046,75606698,47300557,203951463,58885392,35675766,94047111,3071053,4623799,17881424,26067561,9534590,5584795,8146883,6960985,11514860,116922432,71030141,50357314,180327910,62685268,88366071,3950501,245135590,33506680,140836325,4924204,15769017,23724167,37984644,5244115,5947593,44428377,19649020,100089021,9239337,29554783,80475310,8407260,57072137,13475351,7183492,3169265,2794299,164387337,7650051,4341711,109877739,48805148,285518438,22990693,120609957,223623543,3709472,136540575,64675496,4771639,16792101,26898887,25261851,10153668,34574286,40442237,6536419,91162830,20923221,6137730,78003381,55314351,191784837,68845961,73283013,32471914,103252233,60755844,149830100,45842100,51958541,85654236,36812186,43058031,19041256,2623791,332276342,97021591,113346113,10812893,30496923,5411776,174853162,260577854,14349874,132373756,2975884,4480536,124412038,28641651,3594527,11882728,3375207,185970177,216869333,6745365,4207184,39194272,7894563,230582101,145264850,5763342,22279836,2707704,3828091,159387120,410190395,9839267,197776890,8953217,2883663,13905751,11158365,3270616,169541181,12262333,303407934,276959225,210313815,24480974,4076824,27756639,585189062,13058253,363764189,15281050,2542478,18452249,237750805,20276138,16272536,294332358,252742534,2130583863,462185550,268647915,17328222,386315419,352964365,312752266,2313344,874357947,505177649,342471031,435457062,374878495,322372603,2463684,1348207122,696492758,535880669,2172176,398083245,476120461,1061088123,638637072,1903342123,759025196,2039621,422645434,2241650,551873246,2387331,448634113,977298116,490448053,1690401179,1181956121,2038126651,716804021,620344619,803443341,520318678,676701158,568306117,924600596,1493527858,1816328538,1120194691,1004588004,2104856,850135566,1279659514,1947720708,602531930,657419257,737644875,1976407,899176673,1649701506,1090312381,1383509600,780954783,826500455,950638308,1032515596,1992655788,1246411823,1570275450,1419498079,2084110791,1150740857,1859538985,1213845117,1915153,1773726365,1456171521,1313590710,1609656898,1531563934,1731746439
                                            };


template<int N, typename BitChance> class MultiscaleBitChanceTable {
public:
    typedef typename BitChance::Table SubTable;
    SubTable subTable[N];

    MultiscaleBitChanceTable(int cut = 8) {
        for (int i= 0; i<N; i++) {
            subTable[i].init(cut, MULTISCALE_ALPHAS[i]);
          }
    }
};

template<int N, typename BitChance> class MultiscaleBitChance
{
protected:
    BitChance chances[N];
    uint32_t quality[N];
    uint8_t best;
#ifdef STATS
    uint64_t virtSize[N];
    uint64_t realSize;
    uint64_t symbols;
#endif

public:
    typedef MultiscaleBitChanceTable<N,BitChance> Table;

    MultiscaleBitChance(int chanceIn = 0x800) {
        set(chanceIn);
    }

    void set(uint16_t chanceIn) {
        for (int i = 0; i<N; i++) {
            chances[i].set(chanceIn);
            quality[i] = 0;
#ifdef STATS
            virtSize[i] = 0;
#endif
        }
        best = 0;
#ifdef STATS
        symbols = 0;
        realSize = 0;
#endif
    }

    uint16_t get() const {
        return chances[best].get();
    }

    void put(bool bit, const Table &table) {
#ifdef STATS
        int oldBest = best;
        symbols++;
#endif
/*        if (bit == 0)  {
          for (int i=0; i<N; i++) {
            uint64_t sbits = 0;
            chances[i].estim(0, sbits);
            uint64_t oqual=quality[i];
            quality[i] = (oqual*255 + sbits*4097 + 128)>>8;
            chances[i].put(0, table.subTable[i]);
          }
        } else {
          for (int i=0; i<N; i++) {
            uint64_t sbits = 0;
            chances[i].estim(1, sbits);
            uint64_t oqual=quality[i];
            quality[i] = (oqual*255 + sbits*4097 + 128)>>8;
            chances[i].put(1, table.subTable[i]);
          }
        }
*/
        
        for (int i=0; i<N; i++) { // for each scale
            uint64_t sbits = 0;
            chances[i].estim(bit, sbits); // number of bits if this scale was used
            uint64_t oqual=quality[i]; // previous estimate of bits used for this scale
// update quality estimate -- gradually forget the past
//            quality[i] = (oqual*4095 + sbits*65537+2048)/4096; // update bits estimate (([0-2**32-1]*4095+[0..2**16-1]*65537+2048)/4096 = [0..2**32-1])
//            quality[i] = (oqual*2047 + sbits*32769+1024)/2048;
//            quality[i] = (oqual*511 + sbits*8193 + 256)>>9;
            quality[i] = (oqual*255 + sbits*4097 + 128)>>8;
//            quality[i] = (oqual*127 + sbits*2049 + 64)>>7;
//            if (quality[i] < quality[best]) best=i;
            chances[i].put(bit, table.subTable[i]);
#ifdef STATS
            virtSize[i] += sbits;
            if (i == oldBest) realSize += sbits;
#endif
        }

        for (int i=0; i<N; i++) if (quality[i] < quality[best]) best=i;
    }

    void estim(bool bit, uint64_t &total) const {
        chances[best].estim(bit, total);
    }

    int scale() const {
        return chances[0].scale();
    }

#ifdef STATS
    void dist(std::vector<double> &ret) const {
        if (ret.size() != N+1)
            ret = std::vector<double>(N+1, 0.0);

        ret[0] += (double)realSize/scale();
        for (int i=0; i<N; i++)
            ret[i+1] += (double)virtSize[i]/scale();
    }

    void info_bitchance() const {
        printf("%llu bits: ", (unsigned long long)symbols);
        printf("%.5f [", (double)symbols*scale()/realSize);
        for (int i=0; i<N; i++)
            printf("%.4f ", (double)symbols*scale()/virtSize[i]);
        printf("]\n");
    }
#endif
};
