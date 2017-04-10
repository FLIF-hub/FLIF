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

#pragma once

#include "flif-interface-private_common.hpp"
#include "../flif-dec.hpp"

struct FLIF_DECODER
{
    FLIF_DECODER();

    int32_t decode_file(const char* filename);
    int32_t decode_memory(const void* buffer, size_t buffer_size_bytes);
    int32_t abort();
    size_t num_images();
    int32_t num_loops();
    FLIF_IMAGE* get_image(size_t index);

    flif_options options;
    void* callback;
    void* user_data;
    int32_t first_quality;
    ~FLIF_DECODER() {
        // get rid of palettes
        if (internal_images.size()) internal_images[0].clear();
        if (images.size()) images[0].clear();
    }

private:
    Images internal_images;
    Images images;
    std::vector<std::unique_ptr<FLIF_IMAGE>> requested_images;
    bool working;
};
