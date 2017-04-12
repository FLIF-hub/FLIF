#pragma once

#include "image.hpp"

#ifdef HAS_ENCODER
bool image_load_rggb(const char *filename, Image& image, metadata_options &md);
#endif
bool image_save_rggb(const char *filename, const Image& image);
