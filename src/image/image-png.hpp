#pragma once

#include "image.hpp"

#ifdef HAS_ENCODER
int image_load_png(const char *filename, Image &image, metadata_options &md);
#endif
int image_save_png(const char *filename, const Image &image);
