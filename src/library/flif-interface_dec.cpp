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

FLIF_DECODER::FLIF_DECODER()
: quality(100)
, scale(1)
, callback(NULL)
, first_quality(0)
, rw(0)
, rh(0)
, crc_check(0)
, working(false)
{ }


int32_t FLIF_DECODER::decode_file(const char* filename) {
    internal_images.clear();
    images.clear();

    FILE *file = fopen(filename,"rb");
    if(!file)
        return 0;
    FileIO fio(file, filename);

    working = true;
    if(!flif_decode(fio, internal_images, quality, scale, reinterpret_cast<uint32_t (*)(int32_t,int64_t)>(callback), first_quality, images, rw, rh, crc_check))
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
    if(!flif_decode(reader, internal_images, quality, scale, reinterpret_cast<uint32_t (*)(int32_t,int64_t)>(callback), first_quality, images, rw, rh, crc_check))
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
    if (images[index].rows()) {
        requested_images[index]->image = std::move(images[index]); // moves and invalidates images[index]
    }
    return requested_images[index].get();
}


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
    try
    {
        decoder->crc_check = crc_check;
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_decoder_set_quality(FLIF_DECODER* decoder, int32_t quality) {
    try
    {
        decoder->quality = quality;
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_decoder_set_scale(FLIF_DECODER* decoder, uint32_t scale) {
    try
    {
        decoder->scale = scale;
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_decoder_set_resize(FLIF_DECODER* decoder, uint32_t rw, uint32_t rh) {
    try
    {
        decoder->rw = rw;
        decoder->rh = rh;
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_decoder_set_callback(FLIF_DECODER* decoder, uint32_t (*callback)(int32_t quality, int64_t bytes_read)) {
    try
    {
        decoder->callback = (void*) callback;
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


} // extern "C"
