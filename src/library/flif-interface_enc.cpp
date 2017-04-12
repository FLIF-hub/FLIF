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

#ifdef HAS_ENCODER

#include "flif-interface-private_enc.hpp"
#include "flif-interface_common.cpp"

FLIF_ENCODER::FLIF_ENCODER()
: options(FLIF_DEFAULT_OPTIONS)
{
    options.learn_repeats = TREE_LEARN_REPEATS;
}

void FLIF_ENCODER::add_image(FLIF_IMAGE* image) {
    if (!options.alpha_zero_special) image->image.alpha_zero_special = false;
    images.push_back(image->image.clone()); // make a clone so the library user can safely destroy an image after adding it
}


void FLIF_ENCODER::add_image_move(FLIF_IMAGE* image) {
    if (!options.alpha_zero_special) image->image.alpha_zero_special = false;
    images.emplace_back(std::move(image->image)); // variant without cloning, will destroy input image during encoding
    image = NULL;
}


void FLIF_ENCODER::transformations(std::vector<std::string> &desc) {
    uint64_t nb_pixels = (uint64_t)images[0].rows() * images[0].cols();
    if (options.method.o == Optional::undefined) {
        // no method specified, pick one heuristically
        if (nb_pixels * images.size() < 10000) options.method.encoding=flifEncoding::nonInterlaced; // if the image is small, not much point in doing interlacing
        else options.method.encoding=flifEncoding::interlaced; // default method: interlacing
    }
    if (images[0].palette) {
       desc.push_back("Palette_Alpha");  // force palette (including alpha)
       options.keep_palette = 1;
       return;
    }
    if (nb_pixels > 2) {         // no point in doing anything for 1- or 2-pixel images
     if (options.plc && !options.loss) desc.push_back("Channel_Compact");  // compactify channels
     if (options.ycocg) desc.push_back("YCoCg");  // convert RGB(A) to YCoCg(A)
     else desc.push_back("PermutePlanes");  // permute RGB to GRB
     desc.push_back("Bounds");  // get the bounds of the color spaces
     if (!options.loss) {
       desc.push_back("Palette_Alpha");  // try palette (including alpha)
       desc.push_back("Palette");  // try palette (without alpha)
       if (options.acb) {
         desc.push_back("Color_Buckets");  // try auto color buckets if forced
       }
      }
    }
    desc.push_back("Duplicate_Frame");  // find duplicate frames
    if (!options.loss) { // only if lossless
     if (options.frs) desc.push_back("Frame_Shape");  // get the shapes of the frames
     if (options.lookback) desc.push_back("Frame_Lookback");  // make a "deep" alpha channel (negative values are transparent to some previous frame)
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

    if(!flif_encode(fio, images, desc, options))
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

    if(!flif_encode(io, images, desc, options))
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
    if (interlaced) encoder->options.method.encoding = flifEncoding::interlaced;
    else encoder->options.method.encoding = flifEncoding::nonInterlaced;
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_set_learn_repeat(FLIF_ENCODER* encoder, uint32_t learn_repeats) {
    if (learn_repeats < 100) encoder->options.learn_repeats = learn_repeats;
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_set_auto_color_buckets(FLIF_ENCODER* encoder, uint32_t acb) {
    encoder->options.acb = acb;
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_set_palette_size(FLIF_ENCODER* encoder, int32_t palette_size) {
    encoder->options.palette_size = palette_size;
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_set_lookback(FLIF_ENCODER* encoder, int32_t lookback) {
    encoder->options.lookback = lookback;
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_divisor(FLIF_ENCODER* encoder, int32_t divisor) {
    encoder->options.divisor = divisor;
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_min_size(FLIF_ENCODER* encoder, int32_t min_size) {
    encoder->options.min_size = min_size;
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_split_threshold(FLIF_ENCODER* encoder, int32_t split_threshold) {
    encoder->options.split_threshold = split_threshold;
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_alpha_zero_lossless(FLIF_ENCODER* encoder) {
    encoder->options.alpha_zero_special = 0;
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_set_crc_check(FLIF_ENCODER* encoder, uint32_t crc_check) {
    encoder->options.crc_check = crc_check;
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_lossy(FLIF_ENCODER* encoder, int32_t loss) {
    encoder->options.loss = loss;
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_channel_compact(FLIF_ENCODER* encoder, int32_t plc) {
    encoder->options.plc = plc;
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_ycocg(FLIF_ENCODER* encoder, int32_t ycocg) {
    encoder->options.ycocg = ycocg;
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_frame_shape(FLIF_ENCODER* encoder, int32_t frs) {
    encoder->options.frs = frs;
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_chance_cutoff(FLIF_ENCODER* encoder, int32_t cutoff) {
    encoder->options.cutoff = cutoff;
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_set_chance_alpha(FLIF_ENCODER* encoder, int32_t alpha) {
    encoder->options.alpha = alpha;
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_add_image(FLIF_ENCODER* encoder, FLIF_IMAGE* image) {
    try { encoder->add_image(image); }
    catch(...) {}
}
FLIF_DLLEXPORT void FLIF_API flif_encoder_add_image_move(FLIF_ENCODER* encoder, FLIF_IMAGE* image) {
    try { encoder->add_image_move(image); }
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

#endif