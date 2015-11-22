#pragma once
#include "config.h"
#include "maniac/rac.hpp"

#include "fileio.hpp"

template <typename IO> using RacIn = RacInput24<IO>;

#ifdef HAS_ENCODER
template <typename IO> using RacOut = RacOutput24<IO>;
#endif

#include "maniac/compound.hpp"
#ifdef FAST_BUT_WORSE_COMPRESSION
typedef SimpleBitChance  FLIFBitChanceMeta;
#else
typedef MultiscaleBitChance<6,SimpleBitChance>  FLIFBitChanceMeta;
#endif
