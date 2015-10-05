#ifndef FLIF_IMAGE_PNG_H
#define FLIF_IMAGE_PNG_H 1

#include "image.h"

int image_load_png(const char *filename, Image &image);
int image_save_png(const char *filename, const Image &image);

#endif
