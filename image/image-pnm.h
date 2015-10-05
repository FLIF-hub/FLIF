#ifndef FLIF_IMAGE_PNM_H
#define FLIF_IMAGE_PNM_H 1

#include "image.h"

bool image_load_pnm(const char *filename, Image& image);
bool image_save_pnm(const char *filename, const Image& image);

#endif
