#pragma once

#include "image/color_range.hpp"
#include "transform/factory.hpp"
#include "common.hpp"

template <typename IO>
bool flif_encode(IO& io, Images &images, std::vector<std::string> transDesc =
                 {"YCoCg","Bounds","Palette_Alpha","Palette","Color_Buckets","Duplicate_Frame","Frame_Shape","Frame_Lookback"},
                 flifEncoding encoding = flifEncoding::interlaced, int learn_repeats = 3, int acb = -1, int palette_size = 1024,
                 int lookback = 1, int divisor=CONTEXT_TREE_COUNT_DIV, int min_size=CONTEXT_TREE_MIN_SUBTREE_SIZE, int split_threshold=CONTEXT_TREE_SPLIT_THRESHOLD, int cutoff=2, int alpha=19, int crc_check=-1, int loss=0, int predictor[]=NULL, int invisible_predictor=2);
