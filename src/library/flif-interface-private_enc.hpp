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
    void add_image_move(FLIF_IMAGE* image);
    int32_t encode_file(const char* filename);
    int32_t encode_memory(void** buffer, size_t* buffer_size_bytes);

    flif_options options;

    ~FLIF_ENCODER() {
        // get rid of palette
        if (images.size()) images[0].clear();
    }


private:
    void transformations(std::vector<std::string> &desc);
    void set_options(flif_options &options);
    std::vector<Image> images;
};
