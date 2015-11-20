#pragma once

#include "image.hpp"

#ifdef HAS_ENCODER
bool image_load_pnm(const char *filename, Image& image);
#endif
bool image_save_pnm(const char *filename, const Image& image);
