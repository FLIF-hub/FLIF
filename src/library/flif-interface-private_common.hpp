/*
 FLIF - Free Lossless Image Format
 Copyright (C) 2010-2015  Jon Sneyers & Pieter Wuille, LGPL v3+

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "flif_common.h"

#include "../image/image.hpp"
#include "../fileio.hpp"

#ifdef _MSC_VER
 #ifdef FLIF_BUILD_DLL
  #define FLIF_DLLEXPORT __declspec(dllexport)
 #else
  #define FLIF_DLLEXPORT
 #endif
#else
#define FLIF_DLLEXPORT __attribute__ ((visibility ("default")))
#endif

struct FLIF_IMAGE
{
    FLIF_IMAGE();

    void write_row_RGBA8(uint32_t row, const void* buffer, size_t buffer_size_bytes);
    void read_row_RGBA8(uint32_t row, void* buffer, size_t buffer_size_bytes);

    Image image;
};
