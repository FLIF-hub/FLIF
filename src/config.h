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

// maximum image buffer size to attempt to decode
// (the format supports larger images/animations, but this threshold helps to avoid malicious input to grab too much memory)
// this is one frame of 1000 megapixels 8-bit RGB (it's 5 bytes per pixel because YCoCg uses 2 bytes each for Co/Cg)
// (or 1000 frames of 1 megapixel)
#define MAX_IMAGE_BUFFER_SIZE 1000*1000000ULL*5


/************************/
/* COMPILATION OPTIONS  */
/************************/

// speed / binary size trade-off: 0, 1, 2  (higher number -> bigger and faster binary)
#define LARGE_BINARY 2

#ifndef __clang__
#define USE_SIMD 1
#endif


/*************************************************/
/* OPTIONS TO CHANGE DEFAULT ENCODING PARAMETERS */
/*************************************************/

// more repeats makes encoding more expensive, but results in better trees (smaller files)
#define TREE_LEARN_REPEATS 2

#define DEFAULT_MAX_PALETTE_SIZE 512

// 8 byte improvement needed before splitting a MANIAC leaf node
#define CONTEXT_TREE_SPLIT_THRESHOLD 5461*8*8

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
