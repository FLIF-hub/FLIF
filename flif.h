#ifndef FLIF_H
#define FLIF_H 1

#include "image/image.h"

bool flif_encode(const char* filename, Images &images, std::vector<std::string> transDesc, int encoding, int learn_repeats, int acb, int frame_delay, int palette_size, int lookback);
bool flif_encode(const char* filename, Images &images) {
    std::vector<std::string> desc = {"YIQ", "BND", "PLA", "PLT", "ACB", "DUP", "FRS", "FRA"};
    return flif_encode(filename, images, desc, 2, 3, 1, 100, 512, 1);
}

bool flif_decode(const char* filename, Images &images, int quality, int scale);
bool flif_decode(const char* filename, Images &images) {
    return flif_decode(filename, images, 100, 1);
}

bool handle_encode_arguments(int argc, char **argv, Images &images, int palette_size, int acb, int method, int lookback, int learn_repeats, int frame_delay);
int handle_decode_arguments(char **argv, Images &images, int quality, int scale);

#endif

