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
class TransformFrameDup final : public Transform<IO> {
protected:
    std::vector<int> seen_before;
    uint32_t nb;

    bool undo_redo_during_decode() override { return false; }
    const ColorRanges *meta(Images& images, const ColorRanges *srcRanges) override {
        for (unsigned int fr=0; fr<images.size(); fr++) {
            Image& image = images[fr];
            image.seen_before = seen_before[fr];
        }
        return new DupColorRanges(srcRanges);
    }

    void configure(const int setting) override { nb=setting; }

    bool load(const ColorRanges *, RacIn<IO> &rac) override {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18> coder(rac);
        seen_before.clear();
        seen_before.push_back(-1);
        for (unsigned int i=1; i<nb; i++) seen_before.push_back(coder.read_int(-1,i-1));
        int count=0; for(int i : seen_before) { if(i>=0) count++; } v_printf(5,"[%i]",count);
        return true;
    }

#ifdef HAS_ENCODER
    void save(const ColorRanges *, RacOut<IO> &rac) const override {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18> coder(rac);
        assert(nb == seen_before.size());
        for (unsigned int i=1; i<seen_before.size(); i++) coder.write_int(-1,i-1,seen_before[i]);
        int count=0; for(int i : seen_before) { if(i>=0) count++; } v_printf(5,"[%i]",count);
    }

    bool process(const ColorRanges *srcRanges, const Images &images) override {
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
