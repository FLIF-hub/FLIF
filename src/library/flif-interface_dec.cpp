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

#include "flif-interface-private_dec.hpp"
#include "flif-interface_common.cpp"

#include <functional>

FLIF_DECODER::FLIF_DECODER()
: options(FLIF_DEFAULT_OPTIONS)
, callback(NULL)
, user_data(NULL)
, first_quality(0)
, working(false)
{ options.crc_check = 0; options.keep_palette = 1; }


int32_t FLIF_DECODER::decode_file(const char* filename) {
    internal_images.clear();
    images.clear();

    FILE *file = fopen(filename,"rb");
    if(!file)
        return 0;
    FileIO fio(file, filename);

    working = true;
    metadata_options md_default = {
         true, // icc
         true, // exif
         true, // xmp
    };
    if(!flif_decode(fio, internal_images, reinterpret_cast<callback_t>(callback), user_data, first_quality, images, options, md_default, 0))
        { working = false; return 0; }
    working = false;

    images.clear();
    for (Image& image : internal_images) images.emplace_back(std::move(image));

    return 1;
}

int32_t FLIF_DECODER::decode_memory(const void* buffer, size_t buffer_size_bytes) {
    internal_images.clear();
    images.clear();

    BlobReader reader(reinterpret_cast<const uint8_t*>(buffer), buffer_size_bytes);

    working = true;
    metadata_options md_default = {
		true, // icc
		true, // exif
		true, // xmp
    };
    if(!flif_decode(reader, internal_images, reinterpret_cast<callback_t>(callback), user_data, first_quality, images, options, md_default, 0))
        { working = false; return 0; }
    working = false;

    images.clear();
    for (Image& image : internal_images) images.emplace_back(std::move(image));
    return 1;
}

int32_t FLIF_DECODER::abort() {
      if (working) {
        if (images.size() > 0) images[0].abort_decoding();
        return 1;
      } else return 0;
}

size_t FLIF_DECODER::num_images() {
    return images.size();
}

int32_t FLIF_DECODER::num_loops() {
    return 0; // TODO: return actual loop count of the animation
}

FLIF_IMAGE* FLIF_DECODER::get_image(size_t index) {
    if(index >= images.size())
        return 0;
    if(index >= requested_images.size()) requested_images.resize(images.size());
    if (!requested_images[index].get()) requested_images[index].reset( new FLIF_IMAGE());
    if (images[index].rows() || images[index].metadata.size() > 0) {
        requested_images[index]->image = std::move(images[index]); // moves and invalidates images[index]
    }
    return requested_images[index].get();
}


//=============================================================================


FLIF_INFO::FLIF_INFO()
: width(0)
, height(0)
, channels(0)
, bit_depth(0)
, num_images(0)
{ }


//=============================================================================

/*!
Notes about the C interface:

Only use types known to C.
Use types that are unambiguous across all compilers, like uint32_t.
Each function must have it's call convention set.
Exceptions must be caught no matter what.

*/

//=============================================================================


extern "C" {

FLIF_DLLEXPORT FLIF_DECODER* FLIF_API flif_create_decoder() {
    try
    {
        std::unique_ptr<FLIF_DECODER> decoder(new FLIF_DECODER());
        return decoder.release();
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT int32_t FLIF_API flif_abort_decoder(FLIF_DECODER* decoder) {
    try {
      return decoder->abort();
    } catch(...) {}
    return 0;
}
FLIF_DLLEXPORT void FLIF_API flif_destroy_decoder(FLIF_DECODER* decoder) {
    // delete should never let exceptions out
    delete decoder;
    decoder = NULL;
}

FLIF_DLLEXPORT void FLIF_API flif_decoder_set_crc_check(FLIF_DECODER* decoder, int32_t crc_check) {
    decoder->options.crc_check = crc_check;
}

FLIF_DLLEXPORT void FLIF_API flif_decoder_set_quality(FLIF_DECODER* decoder, int32_t quality) {
    decoder->options.quality = quality;
}

FLIF_DLLEXPORT void FLIF_API flif_decoder_set_scale(FLIF_DECODER* decoder, uint32_t scale) {
    decoder->options.scale = scale;
}

FLIF_DLLEXPORT void FLIF_API flif_decoder_set_resize(FLIF_DECODER* decoder, uint32_t rw, uint32_t rh) {
    decoder->options.resize_width = rw;
    decoder->options.resize_height = rh;
}

FLIF_DLLEXPORT void FLIF_API flif_decoder_set_fit(FLIF_DECODER* decoder, uint32_t rw, uint32_t rh) {
    decoder->options.resize_width = rw;
    decoder->options.resize_height = rh;
    decoder->options.fit = 1;
}

FLIF_DLLEXPORT void FLIF_API flif_decoder_set_callback(FLIF_DECODER* decoder, callback_t callback, void *user_data) {
    try
    {
        decoder->callback = (void*) callback;
        decoder->user_data = user_data;
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_decoder_set_first_callback_quality(FLIF_DECODER* decoder, int32_t quality) {
    try
    {
        decoder->first_quality = quality;
    }
    catch(...) {}
}


/*!
* \return non-zero if the function succeeded
*/
FLIF_DLLEXPORT int32_t FLIF_API flif_decoder_decode_file(FLIF_DECODER* decoder, const char* filename) {
    try
    {
        return decoder->decode_file(filename);
    }
    catch(...) {}
    return 0;
}

/*!
* \return non-zero if the function succeeded
*/
FLIF_DLLEXPORT int32_t FLIF_API flif_decoder_decode_memory(FLIF_DECODER* decoder, const void* buffer, size_t buffer_size_bytes) {
    try
    {
        return decoder->decode_memory(buffer, buffer_size_bytes);
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT size_t FLIF_API flif_decoder_num_images(FLIF_DECODER* decoder) {
    try
    {
        return decoder->num_images();
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT int32_t FLIF_API flif_decoder_num_loops(FLIF_DECODER* decoder) {
    try
    {
        return decoder->num_loops();
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT FLIF_IMAGE* FLIF_API flif_decoder_get_image(FLIF_DECODER* decoder, size_t index) {
    try
    {
        return decoder->get_image(index);
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT void FLIF_API flif_decoder_generate_preview(void *context) {
    try
    {
        auto func = (std::function<void ()> *) context;
        (*func)();
    }
    catch(...) {}
}


FLIF_DLLEXPORT FLIF_INFO* FLIF_API flif_read_info_from_memory(const void* buffer, size_t buffer_size_bytes) {
    try
    {
        std::unique_ptr<FLIF_INFO> info(new FLIF_INFO());

        BlobReader reader(reinterpret_cast<const uint8_t*>(buffer), buffer_size_bytes);

        callback_t callback = NULL;
        void *user_data = NULL;
        int first_quality = 0;
        Images images;

        metadata_options md_default = {
            true, // icc
            true, // exif
            true, // xmp
        };
        flif_options options = FLIF_DEFAULT_OPTIONS;

        if(flif_decode(reader, images, reinterpret_cast<callback_t>(callback), user_data, first_quality, images, options, md_default, info.get()))
        {
            return info.release();
        }
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT void FLIF_API flif_destroy_info(FLIF_INFO* info) {
    try
    {
        delete info;
    }
    catch(...) {}
}

FLIF_DLLEXPORT uint32_t FLIF_API flif_info_get_width(FLIF_INFO* info) {
    try
    {
        return info->width;
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT uint32_t FLIF_API flif_info_get_height(FLIF_INFO* info) {
    try
    {
        return info->height;
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT uint8_t FLIF_API flif_info_get_nb_channels(FLIF_INFO* info) {
    try
    {
        return info->channels;
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT uint8_t FLIF_API flif_info_get_depth(FLIF_INFO* info) {
    try
    {
        return info->bit_depth;
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT size_t FLIF_API flif_info_num_images(FLIF_INFO* info) {
    try
    {
        return info->num_images;
    }
    catch(...) {}
    return 0;
}


} // extern "C"
