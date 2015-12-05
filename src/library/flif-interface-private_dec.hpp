/*
 FLIF - Free Lossless Image Format
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

#pragma once

#include "flif-interface-private_common.hpp"
#include "../flif-dec.hpp"

struct FLIF_DECODER
{
    FLIF_DECODER();

    int32_t decode_file(const char* filename);
    int32_t decode_memory(const void* buffer, size_t buffer_size_bytes);
    size_t num_images();
    int32_t num_loops();
    FLIF_IMAGE* get_image(size_t index);

    int32_t quality;
    uint32_t scale;
    void* callback;
    int32_t first_quality;

private:
    Images internal_images;
    Images images;
    std::vector<std::unique_ptr<FLIF_IMAGE>> requested_images;
};
