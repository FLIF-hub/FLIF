#ifndef _FLIF_H_
#define _FLIF_H_ 1

#include "image/image.h"

bool encode(const char* filename, Image &image, std::vector<std::string> transDesc, int encoding, int learn_repeats);
bool decode(const char* filename, Image &image, int quality, int scale);

bool handle_encode_arguments(int argc, char **argv, Images &images, int palette_size, int acb, int method, int lookback, int learn_repeats, int frame_delay);
int handle_decode_arguments(char **argv, Images &images, int quality, int scale);

#endif

