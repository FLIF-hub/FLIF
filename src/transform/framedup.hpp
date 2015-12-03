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

#include <vector>

#include "transform.hpp"
#include "../maniac/symbol.hpp"



template <typename IO>
class TransformFrameDup final : public Transform<IO> {
protected:
    std::vector<int> seen_before;
    uint32_t nb;

    bool undo_redo_during_decode() { return false; }
    const ColorRanges *meta(Images& images, const ColorRanges *srcRanges) {
        for (unsigned int fr=0; fr<images.size(); fr++) {
            Image& image = images[fr];
            image.seen_before = seen_before[fr];
        }
        return new DupColorRanges(srcRanges);
    }

    void configure(const int setting) { nb=setting; }

    bool load(const ColorRanges *, RacIn<IO> &rac) {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 24> coder(rac);
        seen_before.clear();
        seen_before.push_back(-1);
        for (unsigned int i=1; i<nb; i++) seen_before.push_back(coder.read_int(-1,i-1));
        int count=0; for(int i : seen_before) { if(i>=0) count++; } v_printf(5,"[%i]",count);
        return true;
    }

#ifdef HAS_ENCODER
    void save(const ColorRanges *, RacOut<IO> &rac) const {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 24> coder(rac);
        assert(nb == seen_before.size());
        for (unsigned int i=1; i<seen_before.size(); i++) coder.write_int(-1,i-1,seen_before[i]);
        int count=0; for(int i : seen_before) { if(i>=0) count++; } v_printf(5,"[%i]",count);
    }

    bool process(const ColorRanges *srcRanges, const Images &images) {
        int np=srcRanges->numPlanes();
        nb = images.size();
        seen_before.clear();
        seen_before.resize(nb,-1);
        bool dupes_found=false;
        for (unsigned int fr=1; fr<images.size(); fr++) {
            const Image& image = images[fr];
            for (unsigned int ofr=0; ofr<fr; ofr++) {
              const Image& oimage = images[ofr];
              bool identical=true;
              for (uint32_t r=0; r<image.rows(); r++) {
                for (uint32_t c=0; c<image.cols(); c++) {
                    for (int p=0; p<np; p++) {
                       if(image(p,r,c) != oimage(p,r,c)) { identical=false; break;}
                    }
                    if (!identical) {break;}
                }
                if (!identical) {break;}
              }
              if (identical) {seen_before[fr] = ofr; dupes_found=true; break;}
            }
        }
        return dupes_found;
    }
#endif
};
