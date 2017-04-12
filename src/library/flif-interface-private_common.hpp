/*
FLIF - Free Lossless Image Format

Copyright 2010-2016, Jon Sneyers & Pieter Wuille

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
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

    void write_row_RGBA16(uint32_t row, const void* buffer, size_t buffer_size_bytes);
    void read_row_RGBA16(uint32_t row, void* buffer, size_t buffer_size_bytes);

    void write_row_RGB8(uint32_t row, const void* buffer, size_t buffer_size_bytes);
    void write_row_GRAY8(uint32_t row, const void* buffer, size_t buffer_size_bytes);
    void read_row_GRAY8(uint32_t row, void* buffer, size_t buffer_size_bytes);
    void write_row_PALETTE8(uint32_t row, const void* buffer, size_t buffer_size_bytes);
    void read_row_PALETTE8(uint32_t row, void* buffer, size_t buffer_size_bytes);

    Image image;
};
