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

#include "../image/image.hpp"
#include "../image/color_range.hpp"
#include "../maniac/rac.hpp"
#include "../flif_config.h"


template <typename IO>
class Transform {
protected:

public:
    virtual ~Transform() {};

    // On encode: init, process, save, meta, data, <encoding>
    // On decode: init,          load, meta,       <decoding>, invData           ( + optional configure anywhere)
    // Progressive decode: init, load, meta,       <decoding>, invData, <render>, data,
    //                                             <decoding>, invData, <render>, data,
    //                                             ...
    //                                             <decoding>, invData


    bool virtual init(const ColorRanges *) { return true; }
    bool virtual undo_redo_during_decode() { return true; }
    void virtual configure(const int) { }
    bool virtual load(const ColorRanges *, RacIn<IO> &) { return true; };
#ifdef HAS_ENCODER
    bool virtual process(const ColorRanges *, const Images &) { return true; };
    void virtual save(const ColorRanges *, RacOut<IO> &) const {};
    void virtual data(Images&) const {}
#endif
    const ColorRanges virtual *meta(Images&, const ColorRanges *srcRanges) { return new DupColorRanges(srcRanges); }
    void virtual invData(Images&, uint32_t strideCol=1, uint32_t strideRow=1) const {}
    bool virtual is_palette_transform() const { return false; }
};
