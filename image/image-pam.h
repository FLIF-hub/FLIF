#ifndef _IMAGE_PAM_H_
#define _IMAGE_PAM_H_ 1

#include "image.h"

bool image_load_pam(const char *filename, Image& image);
bool image_save_pam(const char *filename, const Image& image);

#endif
