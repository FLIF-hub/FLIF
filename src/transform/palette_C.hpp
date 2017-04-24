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
#include <tuple>
#include <set>


class ColorRangesPaletteC final : public ColorRanges {
protected:
    const ColorRanges *ranges;
    int nb_colors[4];
public:
    ColorRangesPaletteC(const ColorRanges *rangesIn, const int nb[4]) : ranges(rangesIn) { for (int i=0; i<4; i++) nb_colors[i] = nb[i]; }
    bool isStatic() const override { return true; }
    int numPlanes() const override { return ranges->numPlanes(); }

    ColorVal min(int p) const override { return 0; }
    ColorVal max(int p) const override { return nb_colors[p]; }
    void minmax(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv) const override {
         minv=0; maxv=nb_colors[p];
    }

};


template <typename IO>
class TransformPaletteC : public Transform<IO> {
protected:
    std::vector<ColorVal> CPalette_vector[4];
    std::vector<ColorVal> CPalette_inv_vector[4];

public:
    bool init(const ColorRanges *srcRanges) override {
        if (srcRanges->numPlanes()>4) return false; // FRA should always be done after CC, this is just to catch bad input
        return true;
    }

    const ColorRanges *meta(Images& images, const ColorRanges *srcRanges) override {
        int nb[4] = {};
        v_printf(4,"[");
        for (int i=0; i<srcRanges->numPlanes(); i++) {
            nb[i] = CPalette_vector[i].size()-1;
            if (i>0) v_printf(4,",");
            v_printf(4,"%i",nb[i]);
//            if (nb[i] < 64) nb[i] <<= 2;
        }
        v_printf(4,"]");
        return new ColorRangesPaletteC(srcRanges, nb);
    }

    void invData(Images& images, uint32_t strideCol, uint32_t strideRow) const override {
        for (Image& image : images) {

         const uint32_t scaledRows = image.scaledRows();
         const uint32_t scaledCols = image.scaledCols();

         for (int p=0; p<image.numPlanes(); p++) {
          auto palette = CPalette_vector[p];
          auto palette_size = palette.size();
//          const int stretch = (palette_size > 64 ? 0 : 2);
          image.undo_make_constant_plane(p);
          GeneralPlane &plane = image.getPlane(p);
          for (uint32_t r=0; r<scaledRows; r ++) {
            for (uint32_t c=0; c<scaledCols; c ++) {
                int P=plane.get(r,c);
//                image.set(p,r,c, palette[image(p,r,c) >> stretch]);
                if (P < 0 || P >= (int) palette_size) P = 0; // might happen on invisible pixels with predictor -H1
                assert(P < (int) palette_size);
                plane.set(r,c, palette[P]);
            }
          }
         }
        }
    }

#if HAS_ENCODER
    bool process(const ColorRanges *srcRanges, const Images &images) override {

        if (images[0].palette) return false; // skip if the image is already a palette image

        std::set<ColorVal> CPalette;
        bool nontrivial=false;
        for (int p=0; p<srcRanges->numPlanes(); p++) {
         if (p==3) CPalette.insert(0); // ensure that A=0 is still A=0 even if image does not contain zero-alpha pixels
         for (const Image& image : images) {
          for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                CPalette.insert(image(p,r,c));
            }
          }
         }
//         if ((int)CPalette.size() <= srcRanges->max(p)-srcRanges->min(p)) nontrivial = true;
         // if on all channels, less than 10% of the range can be compacted away, it's probably a bad idea to do the compaction
         // since the gain from a smaller RGB range will probably not compensate for the disturbing of the YIQ transform
         if ((int)CPalette.size() * 10 <= 9 * (srcRanges->max(p)-srcRanges->min(p))) nontrivial = true;
         if (CPalette.size() < 10) {
           // add up to 10 shades of gray
           ColorVal prev=0;
           for (ColorVal c : CPalette) {
             if (c > prev+1) CPalette_vector[p].push_back((c+prev)/2);
             CPalette_vector[p].push_back(c);
             prev=c;
             nontrivial=true;
           }
         } else {
           for (ColorVal c : CPalette) CPalette_vector[p].push_back(c);
         }
         CPalette.clear();
         CPalette_inv_vector[p].resize(srcRanges->max(p)+1);
         for (unsigned int i=0; i<CPalette_vector[p].size(); i++) CPalette_inv_vector[p][CPalette_vector[p][i]]=i;
        }
        return nontrivial;
    }
    void data(Images& images) const override {
        for (Image& image : images) {
         for (int p=0; p<image.numPlanes(); p++) {
//          const int stretch = (CPalette_vector[p].size()>64 ? 0 : 2);
          for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                ColorVal P = CPalette_inv_vector[p][image(p,r,c)];
//                for (ColorVal x : CPalette_vector[p]) {if (x==image(p,r,c)) break; else P++;}
//                image.set(p,r,c, P << stretch);
                image.set(p,r,c, P);
            }
          }
         }
        }
    }
    void save(const ColorRanges *srcRanges, RacOut<IO> &rac) const override {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18> coder(rac);
        for (int p=0; p<srcRanges->numPlanes(); p++) {
          coder.write_int(0, srcRanges->max(p)-srcRanges->min(p), CPalette_vector[p].size()-1);
          ColorVal min=srcRanges->min(p);
          int remaining=CPalette_vector[p].size()-1;
          for (unsigned int i=0; i<CPalette_vector[p].size(); i++) {
            coder.write_int(0, srcRanges->max(p)-min-remaining, CPalette_vector[p][i]-min);
            min = CPalette_vector[p][i]+1;
            remaining--;
          }
        }
    }
#endif
    bool load(const ColorRanges *srcRanges, RacIn<IO> &rac) override {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18> coder(rac);
        for (int p=0; p<srcRanges->numPlanes(); p++) {
          unsigned int nb = coder.read_int(0, srcRanges->max(p)-srcRanges->min(p)) + 1;
          ColorVal min=srcRanges->min(p);
          int remaining = nb-1;
          for (unsigned int i=0; i<nb; i++) {
            CPalette_vector[p].push_back(min + coder.read_int(0, srcRanges->max(p)-min-remaining));
            min = CPalette_vector[p][i]+1;
            remaining--;
          }
        }
        return true;
    }
};
