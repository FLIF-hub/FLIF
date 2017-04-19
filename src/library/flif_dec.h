#ifndef FLIF_INTERFACE_DEC_H
#define FLIF_INTERFACE_DEC_H

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

#include "flif_common.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

    typedef uint32_t (*callback_t)(uint32_t quality, int64_t bytes_read, uint8_t decode_over, void *user_data, void *context);

    typedef struct FLIF_DECODER FLIF_DECODER;
    typedef struct FLIF_INFO FLIF_INFO;

    // initialize a FLIF decoder
    FLIF_DLLIMPORT FLIF_DECODER* FLIF_API flif_create_decoder();

    // decode a given FLIF file
    FLIF_DLLIMPORT int32_t FLIF_API flif_decoder_decode_file(FLIF_DECODER* decoder, const char* filename);
    // decode a FLIF blob in memory: buffer should point to the blob and buffer_size_bytes should be its size
    FLIF_DLLIMPORT int32_t FLIF_API flif_decoder_decode_memory(FLIF_DECODER* decoder, const void* buffer, size_t buffer_size_bytes);

    // returns the number of frames (1 if it is not an animation)
    FLIF_DLLIMPORT size_t FLIF_API flif_decoder_num_images(FLIF_DECODER* decoder);
    // only relevant for animations: returns the loop count (0 = loop forever)
    FLIF_DLLIMPORT int32_t FLIF_API flif_decoder_num_loops(FLIF_DECODER* decoder);
    // returns a pointer to a given frame, counting from 0 (use index=0 for still images)
    FLIF_DLLIMPORT FLIF_IMAGE* FLIF_API flif_decoder_get_image(FLIF_DECODER* decoder, size_t index);

    FLIF_DLLIMPORT void FLIF_API flif_decoder_generate_preview(void *context);

    // release an decoder (has to be called after decoding is done, to avoid memory leaks)
    FLIF_DLLIMPORT void FLIF_API flif_destroy_decoder(FLIF_DECODER* decoder);
    // abort a decoder (can be used before decoding is completed)
    FLIF_DLLIMPORT int32_t FLIF_API flif_abort_decoder(FLIF_DECODER* decoder);

    // decode options, all optional, can be set after decoder initialization and before actual decoding
    FLIF_DLLIMPORT void FLIF_API flif_decoder_set_crc_check(FLIF_DECODER* decoder, int32_t crc_check); // default: no (0)
    FLIF_DLLIMPORT void FLIF_API flif_decoder_set_quality(FLIF_DECODER* decoder, int32_t quality); // valid quality: 0-100
    FLIF_DLLIMPORT void FLIF_API flif_decoder_set_scale(FLIF_DECODER* decoder, uint32_t scale); // valid scales: 1,2,4,8,16,...
    FLIF_DLLIMPORT void FLIF_API flif_decoder_set_resize(FLIF_DECODER* decoder, uint32_t width, uint32_t height);
    FLIF_DLLIMPORT void FLIF_API flif_decoder_set_fit(FLIF_DECODER* decoder, uint32_t width, uint32_t height);

    // Progressive decoding: set a callback function. The callback will be called after a certain quality is reached,
    // and it should return the desired next quality that should be reached before it will be called again.
    // The qualities are expressed on a scale from 0 to 10000 (not 0 to 100!) for fine-grained control.
    // `user_data` can be NULL or a pointer to any user-defined context. The decoder doesn't care about its contents;
    // it just passes the pointer value back to the callback.
    FLIF_DLLIMPORT void FLIF_API flif_decoder_set_callback(FLIF_DECODER* decoder, callback_t callback, void *user_data);
    FLIF_DLLIMPORT void FLIF_API flif_decoder_set_first_callback_quality(FLIF_DECODER* decoder, int32_t quality); // valid quality: 0-10000

    // Reads the header of a FLIF file and packages it as a FLIF_INFO struct.
    // May return a null pointer if the file is not in the right format.
    // The caller takes ownership of the return value and must call flif_destroy_info().
    FLIF_DLLIMPORT FLIF_INFO* FLIF_API flif_read_info_from_memory(const void* buffer, size_t buffer_size_bytes);
    // deallocator function for FLIF_INFO
    FLIF_DLLIMPORT void FLIF_API flif_destroy_info(FLIF_INFO* info);

    // get the image width
    FLIF_DLLIMPORT uint32_t FLIF_API flif_info_get_width(FLIF_INFO* info);
    // get the image height
    FLIF_DLLIMPORT uint32_t FLIF_API flif_info_get_height(FLIF_INFO* info);
    // get the number of color channels
    FLIF_DLLIMPORT uint8_t  FLIF_API flif_info_get_nb_channels(FLIF_INFO* info);
    // get the number of bits per channel
    FLIF_DLLIMPORT uint8_t  FLIF_API flif_info_get_depth(FLIF_INFO* info);
    // get the number of animation frames
    FLIF_DLLIMPORT size_t   FLIF_API flif_info_num_images(FLIF_INFO* info);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // FLIF_INTERFACE_DEC_H
