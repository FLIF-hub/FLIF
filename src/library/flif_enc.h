#ifndef FLIF_INTERFACE_ENC_H
#define FLIF_INTERFACE_ENC_H

/*
 FLIF encoder - Free Lossless Image Format
 Copyright (C) 2010-2015  Jon Sneyers & Pieter Wuille, GPL v3+

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "flif_common.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

    typedef struct FLIF_ENCODER FLIF_ENCODER;

    FLIF_DLLIMPORT FLIF_ENCODER* FLIF_API flif_create_encoder();
    FLIF_DLLIMPORT void FLIF_API flif_destroy_encoder(FLIF_ENCODER* encoder);

    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_interlaced(FLIF_ENCODER* encoder, uint32_t interlaced);
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_learn_repeat(FLIF_ENCODER* encoder, uint32_t learn_repeats);
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_auto_color_buckets(FLIF_ENCODER* encoder, uint32_t acb);
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_palette_size(FLIF_ENCODER* encoder, int32_t palette_size);
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_lookback(FLIF_ENCODER* encoder, int32_t loopback);
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_divisor(FLIF_ENCODER* encoder, int32_t divisor);
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_min_size(FLIF_ENCODER* encoder, int32_t min_size);
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_split_threshold(FLIF_ENCODER* encoder, int32_t split_threshold);

    FLIF_DLLIMPORT void FLIF_API flif_encoder_add_image(FLIF_ENCODER* encoder, FLIF_IMAGE* image);
    FLIF_DLLIMPORT int32_t FLIF_API flif_encoder_encode_file(FLIF_ENCODER* encoder, const char* filename);
    FLIF_DLLIMPORT int32_t FLIF_API flif_encoder_encode_memory(FLIF_ENCODER* encoder, void** buffer, size_t* buffer_size_bytes);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // FLIF_INTERFACE_ENC_H
