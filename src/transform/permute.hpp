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
#include "transform.hpp"

class ColorRangesPermute final : public ColorRanges {
protected:
    const std::vector<int> permutation;
    const ColorRanges *ranges;
public:
    ColorRangesPermute(const std::vector<int> &perm, const ColorRanges *rangesIn)
            : permutation(perm), ranges(rangesIn) { }
    bool isStatic() const override { return false; }
    int numPlanes() const override { return ranges->numPlanes(); }

    ColorVal min(int p) const override { return ranges->min(permutation[p]); }
    ColorVal max(int p) const override { return ranges->max(permutation[p]); }

};

class ColorRangesPermuteSubtract final : public ColorRanges {
protected:
    const std::vector<int> permutation;
    const ColorRanges *ranges;
public:
    ColorRangesPermuteSubtract(const std::vector<int> &perm, const ColorRanges *rangesIn)
            : permutation(perm), ranges(rangesIn) { }
    bool isStatic() const override { return false; }
    int numPlanes() const override { return ranges->numPlanes(); }

    ColorVal min(int p) const override { if (p==0 || p>2) return ranges->min(permutation[p]); else return ranges->min(permutation[p])-ranges->max(permutation[0]); }
    ColorVal max(int p) const override { if (p==0 || p>2) return ranges->max(permutation[p]); else return ranges->max(permutation[p])-ranges->min(permutation[0]); }
    void minmax(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv) const override {
         if (p==0 || p>2) { minv = min(p); maxv = max(p); }
         else { minv = ranges->min(permutation[p])-pp[0]; maxv = ranges->max(permutation[p])-pp[0]; }
    }

};


template <typename IO>
class TransformPermute : public Transform<IO> {
protected:
    std::vector<int> permutation;
    const ColorRanges *ranges;
    bool subtract;

public:
    bool virtual init(const ColorRanges *srcRanges) override {
        if (srcRanges->numPlanes() < 3) return false;
        if (srcRanges->min(0) < 0 || srcRanges->min(1) < 0 || srcRanges->min(2) < 0) return false; // already did YCoCg
        permutation.resize(srcRanges->numPlanes());
        ranges = srcRanges;
        return true;
    }

    const ColorRanges *meta(Images&, const ColorRanges *srcRanges) override {
        if (subtract) return new ColorRangesPermuteSubtract(permutation, srcRanges);
        else return new ColorRangesPermute(permutation, srcRanges);
    }

#ifdef HAS_ENCODER
    void configure(const int setting) override {
        subtract = setting;
    }
    bool process(const ColorRanges *srcRanges, const Images &images) override {
        if (images[0].palette) return false; // skip if the image is already a palette image
        const int perm[5] = {1,0,2,3,4}; // just always transform RGB to GRB, we can do something more complicated later
        for (int p=0; p<srcRanges->numPlanes(); p++) {
            permutation[p] = perm[p];
        }
        return true;
    }
    void data(Images& images) const override {
        ColorVal pixel[5];
        for (Image& image : images)
        for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                for (int p=0; p<ranges->numPlanes(); p++) pixel[p] = image(p,r,c);
                image.set(0,r,c, pixel[permutation[0]]);
                if (!subtract) { for (int p=1; p<ranges->numPlanes(); p++) image.set(p,r,c, pixel[permutation[p]]); }
                else { for (int p=1; p<3 && p<ranges->numPlanes(); p++) image.set(p,r,c, pixel[permutation[p]] - pixel[permutation[0]]);
                       for (int p=3; p<ranges->numPlanes(); p++) image.set(p,r,c, pixel[permutation[p]]); }
            }
        }
    }
    void save(const ColorRanges *srcRanges, RacOut<IO> &rac) const override {
        SimpleSymbolCoder<SimpleBitChance, RacOut<IO>, 18> coder(rac);
        coder.write_int2(0, 1, subtract);
        if (subtract) v_printf(4,"Subtract");
        for (int p=0; p<srcRanges->numPlanes(); p++) {
            coder.write_int2(0, srcRanges->numPlanes()-1, permutation[p]);
            v_printf(5,"[%i->%i]",p,permutation[p]);
        }
    }
#endif
    bool load(const ColorRanges *srcRanges, RacIn<IO> &rac) override {
        SimpleSymbolCoder<SimpleBitChance, RacIn<IO>, 18> coder(rac);
        subtract = coder.read_int2(0, 1);
        if (subtract) v_printf(4,"Subtract");
        bool from[4] = {false, false, false, false}, to[4] = {false, false, false, false};
        for (int p=0; p<srcRanges->numPlanes(); p++) {
            permutation[p] = coder.read_int2(0, srcRanges->numPlanes()-1);
            v_printf(5,"[%i->%i]",p,permutation[p]);
            from[p] = true;
            to[permutation[p]] = true;
        }
        for (int p=0; p<srcRanges->numPlanes(); p++) {
            if (!from[p] || !to[p]) {
                e_printf("\nNot a valid permutation!\n");
                return false;
            }
        }
        return true;
    }
#define CLAMP(x,l,u) (x>u?u:(x<l?l:x))
    void invData(Images& images, uint32_t strideCol, uint32_t strideRow) const override {
        ColorVal pixel[5];
        for (Image& image : images) {

          const uint32_t scaledRows = image.scaledRows();
          const uint32_t scaledCols = image.scaledCols();

          for (int p=0; p<ranges->numPlanes(); p++) image.undo_make_constant_plane(p);
          for (uint32_t r=0; r<scaledRows; r+=strideRow) {
            for (uint32_t c=0; c<scaledCols; c+=strideCol) {
                for (int p=0; p<ranges->numPlanes(); p++) pixel[p] = image(p,r,c);
                for (int p=0; p<ranges->numPlanes(); p++) image.set(permutation[p],r,c, pixel[p]);
                image.set(permutation[0],r,c, pixel[0]);
                if (!subtract) { for (int p=1; p<ranges->numPlanes(); p++) image.set(permutation[p],r,c, pixel[p]); }
                else { for (int p=1; p<3 && p<ranges->numPlanes(); p++) image.set(permutation[p],r,c, CLAMP(pixel[p] + pixel[0], ranges->min(permutation[p]), ranges->max(permutation[p])));
                       for (int p=3; p<ranges->numPlanes(); p++) image.set(permutation[p],r,c, pixel[p]); }
            }
          }
        }
    }
};

