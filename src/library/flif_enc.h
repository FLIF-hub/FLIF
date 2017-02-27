#ifndef FLIF_INTERFACE_ENC_H
#define FLIF_INTERFACE_ENC_H

/*
 FLIF encoder - Free Lossless Image Format
 Copyright (C) 2010-2015  Jon Sneyers & Pieter Wuille, LGPL v3+

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "flif_common.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

    typedef struct FLIF_ENCODER FLIF_ENCODER;

    // initialize a FLIF encoder
    FLIF_DLLIMPORT FLIF_ENCODER* FLIF_API flif_create_encoder();

    // give it an image to encode; add more than one image to encode an animation; it will CLONE the image
    // (so the input image is not touched and you have to call flif_destroy_image on it yourself to free that memory)
    FLIF_DLLIMPORT void FLIF_API flif_encoder_add_image(FLIF_ENCODER* encoder, FLIF_IMAGE* image);

    // give it an image to encode; add more than one image to encode an animation; it will MOVE the input image
    // (input image becomes invalid during encode and flif_destroy_encoder will free it)
    FLIF_DLLIMPORT void FLIF_API flif_encoder_add_image_move(FLIF_ENCODER* encoder, FLIF_IMAGE* image);

    // encode to a file
    FLIF_DLLIMPORT int32_t FLIF_API flif_encoder_encode_file(FLIF_ENCODER* encoder, const char* filename);

    // encode to memory (afterwards, buffer will point to the blob and buffer_size_bytes contains its size)
    FLIF_DLLIMPORT int32_t FLIF_API flif_encoder_encode_memory(FLIF_ENCODER* encoder, void** buffer, size_t* buffer_size_bytes);

    // release an encoder (has to be called to avoid memory leaks)
    FLIF_DLLIMPORT void FLIF_API flif_destroy_encoder(FLIF_ENCODER* encoder);

    // encoder options (these are all optional, the defaults should be fine)
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_interlaced(FLIF_ENCODER* encoder, uint32_t interlaced);      // 0 = -N, 1 = -I (default: -I)
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_learn_repeat(FLIF_ENCODER* encoder, uint32_t learn_repeats); // default: 2 (-R)
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_auto_color_buckets(FLIF_ENCODER* encoder, uint32_t acb);     // 0 = -B, 1 = default
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_palette_size(FLIF_ENCODER* encoder, int32_t palette_size);   // default: 512  (max palette size)
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_lookback(FLIF_ENCODER* encoder, int32_t lookback);           // default: 1 (-L)
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_divisor(FLIF_ENCODER* encoder, int32_t divisor);             // default: 30 (-D)
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_min_size(FLIF_ENCODER* encoder, int32_t min_size);           // default: 50 (-M)
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_split_threshold(FLIF_ENCODER* encoder, int32_t threshold);   // default: 64 (-T)
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_alpha_zero_lossless(FLIF_ENCODER* encoder);                  // 0 = default, 1 = -K
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_chance_cutoff(FLIF_ENCODER* encoder, int32_t cutoff); // default: 2  (-X)
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_chance_alpha(FLIF_ENCODER* encoder, int32_t alpha);   // default: 19 (-Z)
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_crc_check(FLIF_ENCODER* encoder, uint32_t crc_check); // 0 = no CRC, 1 = add CRC
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_channel_compact(FLIF_ENCODER* encoder, uint32_t plc); // 0 = -C, 1 = default
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_ycocg(FLIF_ENCODER* encoder, uint32_t ycocg);         // 0 = -Y, 1 = default
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_frame_shape(FLIF_ENCODER* encoder, uint32_t frs);     // 0 = -S, 1 = default

    //set amount of quality loss, 0 for no loss, 100 for maximum loss, negative values indicate adaptive lossy (second image should be the saliency map)
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_lossy(FLIF_ENCODER* encoder, int32_t loss);           // default: 0 (lossless)



#ifdef __cplusplus
}
#endif // __cplusplus

#endif // FLIF_INTERFACE_ENC_H
