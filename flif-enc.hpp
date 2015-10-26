#pragma once

#include "image/color_range.hpp"
#include "transform/factory.hpp"

template <typename IO>
bool flif_encode(IO& io, Images &images, std::vector<std::string> transDesc = {"YIQ","BND","PLA","PLT","ACB","DUP","FRS","FRA"}, flifEncoding encoding = flifEncoding::interlaced, int learn_repeats = 3, int acb = -1, int frame_delay = 100, int palette_size = 512, int lookback = 1, int divisor=CONTEXT_TREE_COUNT_DIV, int min_size=CONTEXT_TREE_MIN_SUBTREE_SIZE, int split_threshold=CONTEXT_TREE_SPLIT_THRESHOLD);
