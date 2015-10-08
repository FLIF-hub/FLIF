#pragma once
#include "config.h"
#include "maniac/rac.h"

#include "fileio.h"

template <typename IO>
using RacIn = RacInput40<IO>;

#ifdef HAS_ENCODER
template <typename IO>
using RacOut = RacOutput40<IO>;
#endif

//typedef RacInput24 RacIn;
//typedef RacOutput24 RacOut;

#include "maniac/compound.h"
typedef MultiscaleBitChance<6,SimpleBitChance>  FLIFBitChanceMeta;
