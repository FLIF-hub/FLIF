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

#include <vector>

#include "transform.hpp"
#include "../maniac/symbol.hpp"



template <typename IO>
class TransformFrameShape final : public Transform<IO> {
protected:
    std::vector<uint32_t> b;
    std::vector<uint32_t> e;
    uint32_t cols;
    uint32_t nb;

    bool undo_redo_during_decode() override { return false; }

    const ColorRanges *meta(Images& images, const ColorRanges *srcRanges) override {
        uint32_t pos=0;
        for (unsigned int fr=1; fr<images.size(); fr++) {
            Image& image = images[fr];
            if (image.seen_before >= 0) continue;
            for (uint32_t r=0; r<image.rows(); r++) {
               assert(pos<nb);
               image.col_begin[r] = b[pos];
               image.col_end[r] = e[pos];
               pos++;
            }
        }
        return new DupColorRanges(srcRanges);
    }

    void configure(const int setting) override { if (nb==0) nb=setting; else cols=setting; } // ok this is dirty

    bool load(const ColorRanges *, RacIn<IO> &rac) override {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18> coder(rac);
        for (unsigned int i=0; i<nb; i+=1) {b.push_back(coder.read_int(0,cols));}
        for (unsigned int i=0; i<nb; i+=1) {
            e.push_back(cols-coder.read_int(0,cols-b[i]));
            if (e[i] > cols || e[i] < b[i] || e[i] <= 0) {
                e_printf("\nError: FRS transform: invalid end column\n");
                return false;
            }
        }
        return true;
    }

#if HAS_ENCODER
    void save(const ColorRanges *, RacOut<IO> &rac) const override {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18> coder(rac);
        assert(nb == b.size());
        assert(nb == e.size());
        for (unsigned int i=0; i<nb; i+=1) { coder.write_int(0,cols,b[i]); }
        for (unsigned int i=0; i<nb; i+=1) { coder.write_int(0,cols-b[i],cols-e[i]); }
    }

    bool process(const ColorRanges *srcRanges, const Images &images) override {
        if (images.size()<2) return false;
        int np=srcRanges->numPlanes();
        nb = 0;
        cols = images[0].cols();
        for (unsigned int fr=1; fr<images.size(); fr++) {
            const Image& image = images[fr];
            if (image.seen_before >= 0) continue;
            nb += image.rows();
            for (uint32_t r=0; r<image.rows(); r++) {
                bool beginfound=false;
                for (uint32_t c=0; c<image.cols(); c++) {
                    if (image.alpha_zero_special && np>3 && image(3,r,c) == 0 && images[fr-1](3,r,c)==0) continue;
                    for (int p=0; p<np; p++) {
                       if(image(p,r,c) != images[fr-1](p,r,c)) { beginfound=true; break;}
                    }
                    if (beginfound) {b.push_back(c); break;}
                }
                if (!beginfound) {b.push_back(image.cols()); e.push_back(image.cols()); continue;}
                bool endfound=false;
                for (uint32_t c=image.cols()-1; c >= b.back(); c--) {
                    if (image.alpha_zero_special && np>3 && image(3,r,c) == 0 && images[fr-1](3,r,c)==0) continue;
                    for (int p=0; p<np; p++) {
                       if(image(p,r,c) != images[fr-1](p,r,c)) { endfound=true; break;}
                    }
                    if (endfound) {e.push_back(c+1); break;}
                }
                if (!endfound) {e.push_back(0); continue;} //shouldn't happen, right?
            }
        }
        /* does not seem to do much good at all
        if (nb&1) {b.push_back(b[nb-1]); e.push_back(e[nb-1]);}
        for (unsigned int i=0; i<nb; i+=2) { b[i]=b[i+1]=std::min(b[i],b[i+1]); }
        for (unsigned int i=0; i<nb; i+=2) { e[i]=e[i+1]=std::max(e[i],e[i+1]); }
        */
        return true;
    }
#endif
};
