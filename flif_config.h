#pragma once
#include "config.h"
#include "maniac/rac.hpp"

#include "fileio.hpp"

#ifdef FAST_BUT_WORSE_COMPRESSION
template <typename IO> using RacIn = RacInput24<IO>;
#else
template <typename IO> using RacIn = RacInput40<IO>;
#endif

#ifdef HAS_ENCODER
#ifdef FAST_BUT_WORSE_COMPRESSION
template <typename IO> using RacOut = RacOutput24<IO>;
#else
template <typename IO> using RacOut = RacOutput40<IO>;
#endif
#endif


#include "maniac/compound.hpp"
#ifdef FAST_BUT_WORSE_COMPRESSION
typedef SimpleBitChance  FLIFBitChanceMeta;
#else
typedef MultiscaleBitChance<6,SimpleBitChance>  FLIFBitChanceMeta;
#endif
