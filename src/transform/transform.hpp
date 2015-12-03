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
    void virtual invData(Images&) const {}

};
