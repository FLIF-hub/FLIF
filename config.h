#pragma once

// define this flag if you want support for > 8 bit per channel
//#define SUPPORT_HDR 1

// output the first K zoomlevels without building trees (too little data anyway to learn much from it)
#define NB_NOLEARN_ZOOMS 12
// this is enough to get a reasonable thumbnail/icon before the tree gets built/transmitted (at most 64x64 pixels)

// more repeats makes encoding more expensive, but results in better trees (smaller files)
#define TREE_LEARN_REPEATS 3


// during decode, check for unexpected file end and interpolate from there
#define CHECK_FOR_BROKENFILES 1

// include encoder related functionality in build. Diable when only interested in decoder
#define HAS_ENCODER  1

