#pragma once

#include "image.h"

bool image_load_pnm(const char *filename, Image& image);
bool image_save_pnm(const char *filename, const Image& image);
