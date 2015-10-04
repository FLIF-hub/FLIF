#ifndef _FLIF_H_
#define _FLIF_H_ 1

#include "image/image.h"

bool encode(const char* filename, Image &image, std::vector<std::string> transDesc, int encoding, int learn_repeats);
bool decode(const char* filename, Image &image, int quality, int scale);

#endif

