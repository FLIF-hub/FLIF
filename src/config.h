#pragma once

/*****************************************/
/* OPTIONS TO DISABLE SOME FUNCTIONALITY */
/*****************************************/

// define this flag if you want support for > 8 bit per channel
#ifndef ONLY_8BIT
#define SUPPORT_HDR 1
#endif

// include encoder related functionality in build. Disable when only interested in decoder
#ifndef DECODER_ONLY
#define HAS_ENCODER  1
#endif

// support animations
#ifndef STILL_ONLY
#define SUPPORT_ANIMATION  1
#endif


// during decode, check for unexpected file end and interpolate from there
#define CHECK_FOR_BROKENFILES 1

// maximum image buffer size to attempt to decode
// (the format supports larger images/animations, but this threshold helps to avoid malicious input to grab too much memory)
// this is one frame of 1000 megapixels 8-bit RGB (it's 5 bytes per pixel because YCoCg uses 2 bytes each for Co/Cg)
// (or 1000 frames of 1 megapixel)
#define MAX_IMAGE_BUFFER_SIZE 1000ULL*1000000ULL*5ULL

// refuse to decode something which claims to have more frames than this
#define MAX_FRAMES 50000

/************************/
/* COMPILATION OPTIONS  */
/************************/

// speed / binary size trade-off: 0, 1, 2  (higher number -> bigger and faster binary)
#define LARGE_BINARY 1

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



// DEFAULT ENCODE/DECODE OPTIONS ARE DEFINED BELOW


#include <vector>
#include <stdint.h>

enum class Optional : uint8_t {
  undefined = 0
};

enum class flifEncoding : uint8_t {
  nonInterlaced = 1,
  interlaced = 2
};

union flifEncodingOptional {
  flifEncoding encoding;
  Optional o;
  flifEncodingOptional() : o(Optional::undefined) {}
};

struct flif_options {
#ifdef HAS_ENCODER
    int learn_repeats;
    int acb;
    std::vector<int> frame_delay;
    int palette_size;
    int lookback;
    int divisor;
    int min_size;
    int split_threshold;
    int ycocg;
    int subtract_green;
    int plc;
    int frs;
    int alpha_zero_special;
    int loss;
    int adaptive;
    int predictor[5];
    int chroma_subsampling;
#endif
    flifEncodingOptional method;
    int invisible_predictor;
    int alpha;
    int cutoff;
    int crc_check;
    int metadata;
    int color_profile;
    int quality;
    int scale;
    int resize_width;
    int resize_height;
    int fit;
    int overwrite;
    int just_add_loss;
    int show_breakpoints;
    int no_full_decode;
    int keep_palette;
};

const struct flif_options FLIF_DEFAULT_OPTIONS = {
#ifdef HAS_ENCODER
    -1, // learn_repeats
    -1, // acb, try auto color buckets
    {100}, // frame_delay
    -1, // palette_size
    1, // lookback
    CONTEXT_TREE_COUNT_DIV, // divisor
    CONTEXT_TREE_MIN_SUBTREE_SIZE, // min_size
    CONTEXT_TREE_SPLIT_THRESHOLD, // split_threshold
    1, // ycocg
    1, // subtract_green
    1, // plc
    1, // frs
    1, // alpha_zero_special
    0, // loss
    0, // adaptive
    {-2,-2,-2,-2,-2}, // predictor, heuristically pick a fixed predictor on all planes
    0, // chroma_subsampling
#endif
    flifEncodingOptional(), // method
    2, // invisible_predictor
    19, // alpha
    2, // cutoff
    -1, // crc_check
    1, // metadata
    1, // color_profile
    100, // quality, 100 = everything, positive value: partial decode, negative value: only rough data
    1, // scale
    0, // resize_width
    0, // resize_height
    0, // fit
    0, // overwrite
    0, // just_add_loss
    0, // show_breakpoints
    0, // no_full_decode
    0, // keep_palette
};
