#ifndef _IMAGE_PNM_H_
#define _IMAGE_PNM_H_ 1

#include "image.h"

bool image_load_pnm(const char *filename, Image& image);
bool image_save_pnm(const char *filename, const Image& image);

#endif
