#ifndef FLIF_H
#define FLIF_H 1

#include "image/image.h"

#include "flif-enc.h"
#include "flif-dec.h"

bool handle_encode_arguments(int argc, char **argv, Images &images, int palette_size, int acb, int method, int lookback, int learn_repeats, int frame_delay);
int handle_decode_arguments(char **argv, Images &images, int quality, int scale);

#endif

