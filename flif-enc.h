#pragma once

#include "image/color_range.h"
#include "transform/factory.h"

template <typename IO>
bool flif_encode(IO& io, Images &images, std::vector<std::string> transDesc, flifEncoding encoding, int learn_repeats, int acb, int frame_delay, int palette_size, int lookback);
