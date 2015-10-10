#pragma once

#include "image/color_range.h"
#include "transform/factory.h"

template <typename IO>
bool flif_encode(IO& io, Images &images, std::vector<std::string> transDesc = {"YIQ","BND","PLA","PLT","ACB","DUP","FRS","FRA"}, flifEncoding encoding = flifEncoding::interlaced, int learn_repeats = 3, int acb = -1, int frame_delay = 100, int palette_size = 512, int lookback = 1);
