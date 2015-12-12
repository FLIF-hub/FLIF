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
#include "transform.hpp"
#include <tuple>
#include <set>

#define MAX_PALETTE_SIZE 30000

class ColorRangesPaletteA final : public ColorRanges {
protected:
    const ColorRanges *ranges;
    int nb_colors;
public:
    ColorRangesPaletteA(const ColorRanges *rangesIn, const int nb) : ranges(rangesIn), nb_colors(nb) { }
    bool isStatic() const { return false; }
    int numPlanes() const { return ranges->numPlanes(); }

    ColorVal min(int p) const { if (p<3) return 0; else if (p==3) return 1; else return ranges->min(p); }
    ColorVal max(int p) const { switch(p) {
                                        case 0: return 0;
                                        case 1: return nb_colors-1;
                                        case 2: return 0;
                                        case 3: return 1;
                                        default: return ranges->max(p);
                                         };
                              }
    void minmax(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv) const {
         if (p==1) { minv=0; maxv=nb_colors-1; return;}
         else if (p<3) { minv=0; maxv=0; return;}
         else if (p==3) { minv=1; maxv=1; return;}
         else ranges->minmax(p,pp,minv,maxv);
    }

};


template <typename IO>
class TransformPaletteA : public Transform<IO> {
protected:
    typedef std::tuple<ColorVal,ColorVal,ColorVal,ColorVal> Color;
    std::vector<Color> Palette_vector;
    unsigned int max_palette_size;
    bool alpha_zero_special;
    bool ordered_palette;

public:
    // dirty hack: max_palette_size is ignored at decode time, alpha_zero_special will be set at encode time
    void configure(const int setting) {
        alpha_zero_special = setting;
        if (setting>0) { ordered_palette=true; max_palette_size = setting;}
        else {ordered_palette=false; max_palette_size = -setting;}
    }
    bool init(const ColorRanges *srcRanges) {
        if (srcRanges->numPlanes() < 4) return false;
        if (srcRanges->min(3) == srcRanges->max(3)) return false; // don't try this if the alpha plane is not actually used
        return true;
    }

    const ColorRanges *meta(Images& images, const ColorRanges *srcRanges) {
        for (Image& image : images) image.palette=true;
        return new ColorRangesPaletteA(srcRanges, Palette_vector.size());
    }

    void invData(Images& images) const {
        for (Image& image : images) {
          image.undo_make_constant_plane(0);
          image.undo_make_constant_plane(1);
          image.undo_make_constant_plane(2);
          image.undo_make_constant_plane(3);
          for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                int P=image(1,r,c);
                image.set(0,r,c, std::get<1>(Palette_vector[P]));
                image.set(1,r,c, std::get<2>(Palette_vector[P]));
                image.set(2,r,c, std::get<3>(Palette_vector[P]));
                image.set(3,r,c, std::get<0>(Palette_vector[P]));
            }
          }
          image.palette=false;
        }
    }

#if HAS_ENCODER
    bool process(const ColorRanges *srcRanges, const Images &images) {
        if (images[0].alpha_zero_special) alpha_zero_special = true; else alpha_zero_special = false;
        if (ordered_palette) {
          std::set<Color> Palette;
          for (const Image& image : images)
          for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                int Y=image(0,r,c), I=image(1,r,c), Q=image(2,r,c), A=image(3,r,c);
                if (alpha_zero_special && A==0) { Y=I=Q=0; }
                Palette.insert(Color(A,Y,I,Q));  // alpha first so sorting makes more sense (?)
                if (Palette.size() > max_palette_size) return false;
            }
          }
          for (Color c : Palette) Palette_vector.push_back(c);
        } else {
          for (const Image& image : images)
          for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                int Y=image(0,r,c), I=image(1,r,c), Q=image(2,r,c), A=image(3,r,c);
                if (alpha_zero_special && A==0) { Y=I=Q=0; }
                Color C(A,Y,I,Q);
                bool found=false;
                for (Color c : Palette_vector) if (c==C) {found=true; break;}
                if (!found) {
                    Palette_vector.push_back(C);
                    if (Palette_vector.size() > max_palette_size) return false;
                }
            }
          }
        }
        uint64_t max_nb_colors = 1;
        for (int p=0; p<4; p++) max_nb_colors *= 1+srcRanges->max(p)-srcRanges->min(p);
        if (Palette_vector.size() == max_nb_colors) return false; // don't make a trivial palette
//        printf("Palette size: %lu\n",Palette.size());
        return true;
    }
    void data(Images& images) const {
//        printf("TransformPalette::data\n");
        for (Image& image : images) {
          for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                Color C(image(3,r,c), image(0,r,c), image(1,r,c), image(2,r,c));
                if (alpha_zero_special && std::get<0>(C) == 0) { std::get<1>(C) = std::get<2>(C) = std::get<3>(C) = 0; }
                ColorVal P=0;
                for (Color c : Palette_vector) {if (c==C) break; else P++;}
//                image.set(0,r,c, 0);
                image.set(1,r,c, P);
//                image.set(2,r,c, 0);
                image.set(3,r,c, 1);
            }
          }
          image.make_constant_plane(0,0);
          image.make_constant_plane(2,0);
          image.make_constant_plane(3,1);
        }
    }
    void save(const ColorRanges *srcRanges, RacOut<IO> &rac) const {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18> coder(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18> coderY(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18> coderI(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18> coderQ(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18> coderA(rac);
        coder.write_int(1, MAX_PALETTE_SIZE, Palette_vector.size());
        prevPlanes pp(2);
        int sorted=1;
        coder.write_int(0, 1, sorted);
        if (sorted) {
            Color min(srcRanges->min(3), srcRanges->min(0), srcRanges->min(1), srcRanges->min(2));
            Color max(srcRanges->max(3), srcRanges->max(0), srcRanges->max(1), srcRanges->max(2));
            Color prev(-1,-1,-1,-1);
            for (Color c : Palette_vector) {
                ColorVal A=std::get<0>(c);
                coderA.write_int(std::get<0>(min), std::get<0>(max), A);
                if (alpha_zero_special && std::get<0>(c) == 0) continue;
                ColorVal Y=std::get<1>(c);
                coderY.write_int((std::get<0>(prev) == A ? std::get<1>(prev) : std::get<1>(min)), std::get<1>(max), Y);
                pp[0]=Y; srcRanges->minmax(1,pp,std::get<2>(min), std::get<2>(max));
                ColorVal I=std::get<2>(c);
                coderI.write_int(std::get<2>(min), std::get<2>(max), I);
                pp[1]=I; srcRanges->minmax(2,pp,std::get<3>(min), std::get<3>(max));
                coderQ.write_int(std::get<3>(min), std::get<3>(max), std::get<3>(c));

                std::get<0>(min) = std::get<0>(c);
                prev = c;
            }
        } else {
            ColorVal min, max;
            for (Color c : Palette_vector) {
                ColorVal A=std::get<0>(c);
                coderA.write_int(srcRanges->min(3),srcRanges->max(3), A);
                if (alpha_zero_special && std::get<0>(c) == 0) continue;
                srcRanges->minmax(0,pp,min,max);
                ColorVal Y=std::get<1>(c);
                coderY.write_int(min,max,Y);
                pp[0]=Y; srcRanges->minmax(1,pp,min,max);
                ColorVal I=std::get<2>(c);
                coderI.write_int(min, max, I);
                pp[1]=I; srcRanges->minmax(2,pp,min,max);
                coderQ.write_int(min, max, std::get<3>(c));
//                printf("YIQ(%i,%i,%i)\t", std::get<0>(c), std::get<1>(c), std::get<2>(c));
            }
        }
//        printf("\nSaved palette of size: %lu\n",Palette_vector.size());
        v_printf(5,"[%lu]",Palette_vector.size());
    }
#endif
    bool load(const ColorRanges *srcRanges, RacIn<IO> &rac) {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18> coder(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18> coderY(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18> coderI(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18> coderQ(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18> coderA(rac);
        long unsigned size = coder.read_int(1, MAX_PALETTE_SIZE);
//        printf("Loading %lu colors: ", size);
        prevPlanes pp(2);
        int sorted = coder.read_int(0,1);
        if (sorted) {
            Color min(srcRanges->min(3), srcRanges->min(0), srcRanges->min(1), srcRanges->min(2));
            Color max(srcRanges->max(3), srcRanges->max(0), srcRanges->max(1), srcRanges->max(2));
            Color prev(-1,-1,-1,-1);
            for (unsigned int p=0; p<size; p++) {
                ColorVal A=coderA.read_int(std::get<0>(min), std::get<0>(max));
                if (alpha_zero_special && A == 0) { Palette_vector.push_back(Color(0,0,0,0)); continue; }
                ColorVal Y=coderY.read_int((std::get<0>(prev) == A ? std::get<1>(prev) : std::get<1>(min)), std::get<1>(max));
                pp[0]=Y; srcRanges->minmax(1,pp,std::get<2>(min), std::get<2>(max));
                ColorVal I=coderI.read_int(std::get<2>(min), std::get<2>(max));
                pp[1]=I; srcRanges->minmax(2,pp,std::get<3>(min), std::get<3>(max));
                ColorVal Q=coderQ.read_int(std::get<3>(min), std::get<3>(max));
                Color c(A,Y,I,Q);
                Palette_vector.push_back(c);
                std::get<0>(min) = std::get<0>(c);
                prev = c;
            }
        } else {
            ColorVal min, max;
            for (unsigned int p=0; p<size; p++) {
                ColorVal A=coderA.read_int(srcRanges->min(3),srcRanges->max(3));
                if (alpha_zero_special && A == 0) { Palette_vector.push_back(Color(0,0,0,0)); continue; }
                srcRanges->minmax(0,pp,min,max);
                ColorVal Y=coderY.read_int(min,max);
                pp[0]=Y; srcRanges->minmax(1,pp,min,max);
                ColorVal I=coderI.read_int(min,max);
                pp[1]=I; srcRanges->minmax(2,pp,min,max);
                ColorVal Q=coderQ.read_int(min,max);
                Color c(A,Y,I,Q);
                Palette_vector.push_back(c);
//                printf("YIQ(%i,%i,%i)\t", std::get<0>(c), std::get<1>(c), std::get<2>(c));
            }
        }
//        printf("\nLoaded palette of size: %lu\n",Palette_vector.size());
        v_printf(5,"[%lu]",Palette_vector.size());
        return true;
    }
};
