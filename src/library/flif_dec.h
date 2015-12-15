#ifndef FLIF_INTERFACE_DEC_H
#define FLIF_INTERFACE_DEC_H

/*
 FLIF decoder - Free Lossless Image Format
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

#include <flif_common.h>


#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

    typedef struct FLIF_DECODER FLIF_DECODER;

    FLIF_DLLIMPORT FLIF_DECODER* FLIF_API flif_create_decoder();
    FLIF_DLLIMPORT int32_t FLIF_API flif_abort_decoder(FLIF_DECODER* decoder);
    FLIF_DLLIMPORT void FLIF_API flif_destroy_decoder(FLIF_DECODER* decoder);

    FLIF_DLLIMPORT void FLIF_API flif_decoder_set_quality(FLIF_DECODER* decoder, int32_t quality); // valid quality: 0-100
    FLIF_DLLIMPORT void FLIF_API flif_decoder_set_scale(FLIF_DECODER* decoder, uint32_t scale); // valid scales: 1,2,4,8,16,...
    FLIF_DLLIMPORT void FLIF_API flif_decoder_set_resize(FLIF_DECODER* decoder, uint32_t width, uint32_t height);
    FLIF_DLLIMPORT void FLIF_API flif_decoder_set_callback(FLIF_DECODER* decoder, uint32_t (*callback)(int32_t quality, int64_t bytes_read));
    FLIF_DLLIMPORT void FLIF_API flif_decoder_set_first_callback_quality(FLIF_DECODER* decoder, int32_t quality); // valid quality: 0-10000

    FLIF_DLLIMPORT int32_t FLIF_API flif_decoder_decode_file(FLIF_DECODER* decoder, const char* filename);
    FLIF_DLLIMPORT int32_t FLIF_API flif_decoder_decode_memory(FLIF_DECODER* decoder, const void* buffer, size_t buffer_size_bytes);
    FLIF_DLLIMPORT size_t FLIF_API flif_decoder_num_images(FLIF_DECODER* decoder);
    FLIF_DLLIMPORT int32_t FLIF_API flif_decoder_num_loops(FLIF_DECODER* decoder);
    FLIF_DLLIMPORT FLIF_IMAGE* FLIF_API flif_decoder_get_image(FLIF_DECODER* decoder, size_t index);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // FLIF_INTERFACE_DEC_H
