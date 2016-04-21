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

#pragma once

#include "flif-interface-private_common.hpp"
#include "../flif-enc.hpp"

struct FLIF_ENCODER
{
    FLIF_ENCODER();

    void add_image(FLIF_IMAGE* image);
    int32_t encode_file(const char* filename);
    int32_t encode_memory(void** buffer, size_t* buffer_size_bytes);

    uint32_t interlaced;
    uint32_t learn_repeats;
    uint32_t acb;
    uint32_t frame_delay;
    int32_t palette_size;
    int32_t lookback;
    int32_t divisor;
    int32_t min_size;
    int32_t split_threshold;
    int32_t alpha_zero_special;
    uint32_t crc_check;
    uint32_t channel_compact;
    uint32_t ycocg;
    uint32_t frame_shape;
    int32_t loss;
    int32_t chance_cutoff;
    int32_t chance_alpha;

private:
    void transformations(std::vector<std::string> &desc);
    std::vector<Image> images;
};
