#ifndef __FLIF_ENC_H__
#define __FLIF_ENC_H__

#include "image/color_range.h"
#include "transform/factory.h"

bool encode(const char* filename, Images &images, std::vector<std::string> transDesc, int encoding, int learn_repeats, int acb, int frame_delay, int palette_size, int lookback);

#endif
