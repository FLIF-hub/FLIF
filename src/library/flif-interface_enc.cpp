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

#include "flif-interface-private_enc.hpp"
#include "flif-interface_common.cpp"

FLIF_ENCODER::FLIF_ENCODER()
: interlaced(1)
, learn_repeats(TREE_LEARN_REPEATS)
, acb(1)
, frame_delay(100)
, palette_size(DEFAULT_MAX_PALETTE_SIZE)
, lookback(1)
, divisor(CONTEXT_TREE_COUNT_DIV)
, min_size(CONTEXT_TREE_MIN_SUBTREE_SIZE)
, split_threshold(CONTEXT_TREE_SPLIT_THRESHOLD)
, alpha_zero_special(1)
, crc_check(1)
, channel_compact(1)
, ycocg(1)
, frame_shape(1)
, loss(0)
, chance_cutoff(2)
, chance_alpha(19)
{
}

void FLIF_ENCODER::add_image(FLIF_IMAGE* image) {
    if (!alpha_zero_special) image->image.alpha_zero_special = false;
    images.push_back(image->image.clone()); // make a clone so the library user can safely destroy an image after adding it
}

void FLIF_ENCODER::transformations(std::vector<std::string> &desc) {
    uint64_t nb_pixels = (uint64_t)images[0].rows() * images[0].cols();
    if (nb_pixels > 2) {         // no point in doing anything for 1- or 2-pixel images
     if (channel_compact && !loss) desc.push_back("Channel_Compact");  // compactify channels
     if (ycocg) desc.push_back("YCoCg");  // convert RGB(A) to YCoCg(A)
     desc.push_back("PermutePlanes");  // permute RGB to GRB
     desc.push_back("Bounds");  // get the bounds of the color spaces
     if (!loss) {
       desc.push_back("Palette_Alpha");  // try palette (including alpha)
       desc.push_back("Palette");  // try palette (without alpha)
       if (acb) {
         desc.push_back("Color_Buckets");  // try auto color buckets if forced
       }
      }
    }
    desc.push_back("Duplicate_Frame");  // find duplicate frames
    if (!loss) { // only if lossless
     if (frame_shape) desc.push_back("Frame_Shape");  // get the shapes of the frames
     if (lookback) desc.push_back("Frame_Lookback");  // make a "deep" alpha channel (negative values are transparent to some previous frame)
    }
}

/*!
* \return non-zero if the function succeeded
*/
int32_t FLIF_ENCODER::encode_file(const char* filename) {
    FILE *file = fopen(filename,"wb");
    if(!file)
        return 0;
    FileIO fio(file, filename);

    std::vector<std::string> desc;
    transformations(desc);

    if(!flif_encode(fio, images, desc,
        interlaced != 0 ? flifEncoding::interlaced : flifEncoding::nonInterlaced,
        learn_repeats, acb, palette_size, lookback,
        divisor, min_size, split_threshold, chance_cutoff, chance_alpha, crc_check, loss))
        return 0;

    return 1;
}

/*!
* \return non-zero if the function succeeded
*/
int32_t FLIF_ENCODER::encode_memory(void** buffer, size_t* buffer_size_bytes) {
    BlobIO io;

    std::vector<std::string> desc;
    transformations(desc);

    if(!flif_encode(io, images, desc,
        interlaced != 0 ? flifEncoding::interlaced : flifEncoding::nonInterlaced,
        learn_repeats, acb, palette_size, lookback,
        divisor, min_size, split_threshold, chance_cutoff, chance_alpha, crc_check, loss))
        return 0;

    *buffer = io.release(buffer_size_bytes);
    return 1;
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

FLIF_DLLEXPORT FLIF_ENCODER* FLIF_API flif_create_encoder() {
    try
    {
        std::unique_ptr<FLIF_ENCODER> encoder(new FLIF_ENCODER());
        return encoder.release();
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT void FLIF_API flif_destroy_encoder(FLIF_ENCODER* encoder) {
    // delete should never let exceptions out
    delete encoder;
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_set_interlaced(FLIF_ENCODER* encoder, uint32_t interlaced) {
    try
    {
        encoder->interlaced = interlaced;
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_set_learn_repeat(FLIF_ENCODER* encoder, uint32_t learn_repeats) {
    try
    {
        encoder->learn_repeats = learn_repeats;
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_set_auto_color_buckets(FLIF_ENCODER* encoder, uint32_t acb) {
    try
    {
        encoder->acb = acb;
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_set_palette_size(FLIF_ENCODER* encoder, int32_t palette_size) {
    try
    {
        encoder->palette_size = palette_size;
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_set_lookback(FLIF_ENCODER* encoder, int32_t lookback) {
    try { encoder->lookback = lookback; }
    catch(...) { }
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_divisor(FLIF_ENCODER* encoder, int32_t divisor) {
    try { encoder->divisor = divisor; }
    catch(...) { }
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_min_size(FLIF_ENCODER* encoder, int32_t min_size) {
    try { encoder->min_size = min_size; }
    catch(...) { }
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_split_threshold(FLIF_ENCODER* encoder, int32_t split_threshold) {
    try { encoder->split_threshold = split_threshold; }
    catch(...) { }
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_alpha_zero_lossless(FLIF_ENCODER* encoder) {
    try { encoder->alpha_zero_special = 0; }
    catch(...) { }
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_set_crc_check(FLIF_ENCODER* encoder, uint32_t crc_check) {
    try { encoder->crc_check = crc_check; }
    catch(...) { }
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_lossy(FLIF_ENCODER* encoder, int32_t loss) {
    try { encoder->loss = loss; }
    catch(...) { }
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_channel_compact(FLIF_ENCODER* encoder, int32_t plc) {
    try { encoder->channel_compact = plc; }
    catch(...) { }
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_ycocg(FLIF_ENCODER* encoder, int32_t ycocg) {
    try { encoder->ycocg = ycocg; }
    catch(...) { }
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_frame_shape(FLIF_ENCODER* encoder, int32_t frs) {
    try { encoder->frame_shape = frs; }
    catch(...) { }
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_chance_cutoff(FLIF_ENCODER* encoder, int32_t cutoff) {
    try { encoder->chance_cutoff = cutoff; }
    catch(...) { }
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_chance_alpha(FLIF_ENCODER* encoder, int32_t alpha) {
    try { encoder->chance_alpha = alpha; }
    catch(...) { }
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_add_image(FLIF_ENCODER* encoder, FLIF_IMAGE* image) {
    try
    {
        encoder->add_image(image);
    }
    catch(...) {}
}

/*!
* \return non-zero if the function succeeded
*/
FLIF_DLLEXPORT int32_t FLIF_API flif_encoder_encode_file(FLIF_ENCODER* encoder, const char* filename) {
    try
    {
        return encoder->encode_file(filename);
    }
    catch(...) {}
    return 0;
}

/*!
* \return non-zero if the function succeeded
*/
FLIF_DLLEXPORT int32_t FLIF_API flif_encoder_encode_memory(FLIF_ENCODER* encoder, void** buffer, size_t* buffer_size_bytes) {
    try
    {
        return encoder->encode_memory(buffer, buffer_size_bytes);
    }
    catch(...) {}
    return 0;
}


} // extern "C"
