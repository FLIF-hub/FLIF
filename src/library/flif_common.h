#ifndef FLIF_INTERFACE_COMMON_H
#define FLIF_INTERFACE_COMMON_H

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

#include <stdint.h>
#include <stddef.h>

#ifdef _MSC_VER
#define FLIF_API __cdecl
 #ifdef FLIF_BUILD_DLL
  #define FLIF_DLLIMPORT __declspec(dllexport)
 #elif FLIF_USE_DLL
  #define FLIF_DLLIMPORT __declspec(dllimport)
 #endif
#else
#define FLIF_API
#endif

#ifndef FLIF_DLLIMPORT
#define FLIF_DLLIMPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

    typedef struct FLIF_IMAGE FLIF_IMAGE;

    FLIF_DLLIMPORT FLIF_IMAGE* FLIF_API flif_create_image(uint32_t width, uint32_t height);
    FLIF_DLLIMPORT void FLIF_API flif_destroy_image(FLIF_IMAGE* image);

    FLIF_DLLIMPORT uint32_t FLIF_API flif_image_get_width(FLIF_IMAGE* image);
    FLIF_DLLIMPORT uint32_t FLIF_API flif_image_get_height(FLIF_IMAGE* image);
    FLIF_DLLIMPORT uint8_t  FLIF_API flif_image_get_nb_channels(FLIF_IMAGE* image);
    FLIF_DLLIMPORT uint32_t FLIF_API flif_image_get_frame_delay(FLIF_IMAGE* image);

    FLIF_DLLIMPORT void FLIF_API flif_image_write_row_RGBA8(FLIF_IMAGE* image, uint32_t row, const void* buffer, size_t buffer_size_bytes);
    FLIF_DLLIMPORT void FLIF_API flif_image_read_row_RGBA8(FLIF_IMAGE* image, uint32_t row, void* buffer, size_t buffer_size_bytes);

    FLIF_DLLIMPORT void FLIF_API flif_free_memory(void* buffer);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // FLIF_INTERFACE_COMMON_H
