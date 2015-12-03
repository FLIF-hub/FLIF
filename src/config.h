#pragma once

/*****************************************/
/* OPTIONS TO DISABLE SOME FUNCTIONALITY */
/*****************************************/

// define this flag if you want support for > 8 bit per channel
#define SUPPORT_HDR 1

// include encoder related functionality in build. Disable when only interested in decoder
#ifndef DECODER_ONLY
#define HAS_ENCODER  1
#endif

// during decode, check for unexpected file end and interpolate from there
#define CHECK_FOR_BROKENFILES 1



/*************************************************/
/* OPTIONS TO CHANGE DEFAULT ENCODING PARAMETERS */
/*************************************************/

// more repeats makes encoding more expensive, but results in better trees (smaller files)
#define TREE_LEARN_REPEATS 3

// 5 byte improvement needed before splitting a MANIAC leaf node
#define CONTEXT_TREE_SPLIT_THRESHOLD 5461*8*5

#define CONTEXT_TREE_COUNT_DIV 30
#define CONTEXT_TREE_MIN_SUBTREE_SIZE 50



/**************************************************/
/* DANGER ZONE: OPTIONS THAT CHANGE THE BITSTREAM */
/* If you modify these, the bitstream format      */
/* changes, so it is no longer compatible!        */
/**************************************************/

// output the first K zoomlevels without building trees (too little data anyway to learn much from it)
#define NB_NOLEARN_ZOOMS 12
// this is enough to get a reasonable thumbnail/icon before the tree gets built/transmitted (at most 64x64 pixels)

// faster decoding, less compression (disable multi-scale bitchances, use 24-bit rac)
#define FAST_BUT_WORSE_COMPRESSION 1

// bounds for node counters
#define CONTEXT_TREE_MIN_COUNT 1
#define CONTEXT_TREE_MAX_COUNT 512
