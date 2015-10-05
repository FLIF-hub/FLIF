#ifndef FLIF_IMAGE_PAM_H
#define FLIF_IMAGE_PAM_H 1

#include "image.h"

bool image_load_pam(const char *filename, Image& image);
bool image_save_pam(const char *filename, const Image& image);

#endif
