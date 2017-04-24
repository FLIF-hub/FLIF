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
#include <algorithm>

#define clip(x,l,u)   if ((x) < (l)) {(x)=(l);} else if ((x) > (u)) {(x)=(u);}


class ColorRangesYCC final : public ColorRanges {
protected:
    const int maximum;
    const ColorRanges *ranges;
public:
    ColorRangesYCC(int m, const ColorRanges *rangesIn)
            : maximum(m), ranges(rangesIn) {
    }
    bool isStatic() const { return false; }
    int numPlanes() const { return ranges->numPlanes(); }

    ColorVal min(int p) const {
      switch(p) {
        case 0: return 0;
        case 1: return -maximum;
        case 2: return -maximum;
        default: return ranges->min(p);
      };
    }
    ColorVal max(int p) const {
      switch(p) {
        case 0: return maximum;
        case 1: return maximum;
        case 2: return maximum;
        default: return ranges->max(p);
      };
    }

    void minmax(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv) const {
         if (p<3) { minv=min(p); maxv=max(p); return; }
         else ranges->minmax(p,pp,minv,maxv);
    }
};


template <typename IO>
class TransformYCC : public Transform<IO> {
protected:
    int maximum;
    const ColorRanges *ranges;

public:
    bool virtual init(const ColorRanges *srcRanges) override {
        if (srcRanges->numPlanes() < 3) return false;
        if (srcRanges->min(0) < 0 || srcRanges->min(1) < 0 || srcRanges->min(2) < 0) return false;
        if (srcRanges->min(0) == srcRanges->max(0) || srcRanges->min(1) == srcRanges->max(1) || srcRanges->min(2) == srcRanges->max(2)) return false;
        maximum = std::max(std::max(srcRanges->max(0), srcRanges->max(1)), srcRanges->max(2));
        ranges = srcRanges;
        return true;
    }

    const ColorRanges *meta(Images&, const ColorRanges *srcRanges) override {
        return new ColorRangesYCC(maximum, srcRanges);
    }

#ifdef HAS_ENCODER
    void data(Images& images) const override {
        ColorVal R,G,B,Y,C1,C2;
        for (Image& image : images)
        for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                R=image(0,r,c);
                G=image(1,r,c);
                B=image(2,r,c);

                C1 = R - B;
                C2 = G - B - C1/2;
                Y = G + (-2*C2)/3;

                image.set(0,r,c, Y);
                image.set(1,r,c, C1);
                image.set(2,r,c, C2);
            }
        }
    }
#endif
    void invData(Images& images, uint32_t strideCol, uint32_t strideRow) const override {
        ColorVal R,G,B,Y,C1,C2;
        const ColorVal max[3] = {ranges->max(0), ranges->max(1), ranges->max(2)};
        for (Image& image : images) {
          image.undo_make_constant_plane(0);
          image.undo_make_constant_plane(1);
          image.undo_make_constant_plane(2);

          const uint32_t scaledRows = image.scaledRows();
          const uint32_t scaledCols = image.scaledCols();

          for (uint32_t r=0; r<scaledRows; r+=strideRow) {
            for (uint32_t c=0; c<scaledCols; c+=strideCol) {
                Y=image(0,r,c);
                C1=image(1,r,c);
                C2=image(2,r,c);

                G = Y - (-2*C2)/3;
                clip(G, 0, max[1]);
                B = G - C2 - C1/2;
                clip(B, 0, max[2]);
                R = B + C1;
                clip(R, 0, max[0]);                // clipping only needed in case of lossy/partial decoding

                image.set(0,r,c, R);
                image.set(1,r,c, G);
                image.set(2,r,c, B);
            }
          }
        }
    }
};

