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
#include <algorithm>

#include "transform.hpp"
#include "../maniac/symbol.hpp"

class ColorRangesBounds final : public ColorRanges {
protected:
    const std::vector<std::pair<ColorVal, ColorVal> > bounds;
    const ColorRanges *ranges;
public:
    ColorRangesBounds(const std::vector<std::pair<ColorVal, ColorVal> > &boundsIn, const ColorRanges *rangesIn) : bounds(boundsIn), ranges(rangesIn) {}
    bool isStatic() const override { return false; }
    int numPlanes() const override { return bounds.size(); }
    ColorVal min(int p) const override { assert(p<numPlanes()); return std::max(ranges->min(p), bounds[p].first); }
    ColorVal max(int p) const override { assert(p<numPlanes()); return std::min(ranges->max(p), bounds[p].second); }
    void snap(const int p, const prevPlanes &pp, ColorVal &min, ColorVal &max, ColorVal &v) const override {
        if (p==0 || p==3) { min=bounds[p].first; max=bounds[p].second; } // optimization for special case
        else {
          ranges->snap(p,pp,min,max,v);
          if (min < bounds[p].first) min=bounds[p].first;
          if (max > bounds[p].second) max=bounds[p].second;
          if (min>max) {
           // should happen only if alpha=0 interpolation produces YI combination for which Q range from ColorRangesYIQ is outside bounds
           min=bounds[p].first;
           max=bounds[p].second;
          }
        }
        if(v>max) v=max;
        if(v<min) v=min;
    }
    void minmax(const int p, const prevPlanes &pp, ColorVal &min, ColorVal &max) const override {
        assert(p<numPlanes());
        if (p==0 || p==3) { min=bounds[p].first; max=bounds[p].second; return; } // optimization for special case
        ranges->minmax(p, pp, min, max);
        if (min < bounds[p].first) min=bounds[p].first;
        if (max > bounds[p].second) max=bounds[p].second;
        if (min>max) {
           // should happen only if alpha=0 interpolation produces YI combination for which Q range from ColorRangesYIQ is outside bounds
           min=bounds[p].first;
           max=bounds[p].second;
        }
        assert(min <= max);
    }
};


template <typename IO>
class TransformBounds : public Transform<IO> {
protected:
    std::vector<std::pair<ColorVal, ColorVal> > bounds;

    bool undo_redo_during_decode() override { return false; }

    const ColorRanges *meta(Images&, const ColorRanges *srcRanges) override {
        if (srcRanges->isStatic()) {
            return new StaticColorRanges(bounds);
        } else {
            return new ColorRangesBounds(bounds, srcRanges);
        }
    }

    bool load(const ColorRanges *srcRanges, RacIn<IO> &rac) override {
        if (srcRanges->numPlanes() > 4) return false; // something wrong if Bounds is done on FRA
        SimpleSymbolCoder<SimpleBitChance, RacIn<IO>, 18> coder(rac);
        bounds.clear();
        for (int p=0; p<srcRanges->numPlanes(); p++) {
//            ColorVal min = coder.read_int(0, srcRanges->max(p) - srcRanges->min(p)) + srcRanges->min(p);
//            ColorVal max = coder.read_int(0, srcRanges->max(p) - min) + min;
            ColorVal min = coder.read_int2(srcRanges->min(p), srcRanges->max(p));
            ColorVal max = coder.read_int2(min, srcRanges->max(p));
            if (min > max) return false;
            if (min < srcRanges->min(p)) return false;
            if (max > srcRanges->max(p)) return false;
            bounds.push_back(std::make_pair(min,max));
            v_printf(5,"[%i:%i..%i]",p,min,max);
        }
        return true;
    }

#ifdef HAS_ENCODER
    void save(const ColorRanges *srcRanges, RacOut<IO> &rac) const override {
        SimpleSymbolCoder<SimpleBitChance, RacOut<IO>, 18> coder(rac);
        for (int p=0; p<srcRanges->numPlanes(); p++) {
            ColorVal min = bounds[p].first;
            ColorVal max = bounds[p].second;
//            coder.write_int(0, srcRanges->max(p) - srcRanges->min(p), min - srcRanges->min(p));
//            coder.write_int(0, srcRanges->max(p) - min, max - min);
            coder.write_int2(srcRanges->min(p), srcRanges->max(p), min);
            coder.write_int2(min, srcRanges->max(p), max);
            v_printf(5,"[%i:%i..%i]",p,min,max);
        }
    }

    bool process(const ColorRanges *srcRanges, const Images &images) override {
        if (images[0].palette) return false; // skip if the image is already a palette image
        bounds.clear();
        bool trivialbounds=true;
        int nump=srcRanges->numPlanes();
        for (int p=0; p<nump; p++) {
            ColorVal min = srcRanges->max(p);
            ColorVal max = srcRanges->min(p);
            for (const Image& image : images)
            for (uint32_t r=0; r<image.rows(); r++) {
                for (uint32_t c=0; c<image.cols(); c++) {
                    if (image.alpha_zero_special && nump>3 && p<3 && image(3,r,c)==0) continue; // don't take fully transparent pixels into account
                    ColorVal v = image(p,r,c);
                    if (v < min) min = v;
                    if (v > max) max = v;
                    assert(v <= srcRanges->max(p));
                    assert(v >= srcRanges->min(p));
                }
            }
            if (min > max) min = max = (min+max)/2; // this can happen if the image is fully transparent
            bounds.push_back(std::make_pair(min,max));
            if (min > srcRanges->min(p)) trivialbounds=false;
            if (max < srcRanges->max(p)) trivialbounds=false;
        }
        return !trivialbounds;
    }
#endif
};
