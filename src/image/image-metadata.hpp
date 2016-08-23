#pragma once

#include "image.hpp"

#ifdef HAS_ENCODER
bool image_load_metadata(const char *filename, Image& image, const char *chunkname);
#endif
bool image_save_metadata(const char *filename, const Image& image, const char *chunkname);

