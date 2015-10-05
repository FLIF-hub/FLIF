

// output the first K zoomlevels without building trees (too little data anyway to learn much from it)
#define NB_NOLEARN_ZOOMS 12
// this is enough to get a reasonable thumbnail/icon before the tree gets built/transmitted (at most 64x64 pixels)

// more repeats makes encoding more expensive, but results in better trees (smaller files)
#define TREE_LEARN_REPEATS 3


// during decode, check for unexpected file end and interpolate from there
#define CHECK_FOR_BROKENFILES 1


#include "maniac/rac.h"

#include "fileio.h"

template <typename IO>
using RacIn = RacInput40<IO>;

template <typename IO>
using RacOut = RacOutput40<IO>;

//typedef RacInput24 RacIn;
//typedef RacOutput24 RacOut;

#include "maniac/compound.h"
typedef MultiscaleBitChance<6,SimpleBitChance>  FLIFBitChanceMeta;
