#ifndef _PALETTEA_H_
#define _PALETTEA_H_ 1

#include "../image/image.h"
#include "../image/color_range.h"
#include "transform.h"
#include <tuple>
#include <set>

#define MAX_PALETTE_SIZE 512

class ColorRangesPaletteA : public ColorRanges
{
protected:
    const ColorRanges *ranges;
    int nb_colors;
public:
    ColorRangesPaletteA(const ColorRanges *rangesIn, const int nb) : ranges(rangesIn), nb_colors(nb) { }
    bool isStatic() const { return false; }
    int numPlanes() const { return ranges->numPlanes(); }

    ColorVal min(int p) const { if (p<3) return 0; else if (p==3) return 1; else return ranges->min(p); }
    ColorVal max(int p) const { switch(p) {
                                        case 0: return nb_colors-1;
                                        case 1: return 0;
                                        case 2: return 0;
                                        case 3: return 1;
                                        default: return ranges->max(p);
                                         };
                              }
    void minmax(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv) const {
         if (p==0) { minv=0; maxv=nb_colors-1; return;}
         else if (p<3) { minv=0; maxv=0; return;}
         else if (p==3) { minv=1; maxv=1; return;}
         else ranges->minmax(p,pp,minv,maxv);
    }

};


class TransformPaletteA : public Transform {
protected:
    typedef std::tuple<ColorVal,ColorVal,ColorVal,ColorVal> Color;
    std::set<Color> Palette;
    std::vector<Color> Palette_vector;

public:
    bool virtual init(const ColorRanges *srcRanges) {
        if (srcRanges->numPlanes() < 4) return false;
        return true;
    }

    const ColorRanges *meta(Image& image, const ColorRanges *srcRanges) {
        return new ColorRangesPaletteA(srcRanges, Palette_vector.size());
    }

    bool process(const ColorRanges *srcRanges, const Image &image) {
        for (int r=0; r<image.rows(); r++) {
            for (int c=0; c<image.cols(); c++) {
                int Y=image(0,r,c), I=image(1,r,c), Q=image(2,r,c), A=image(3,r,c);
                if (A==0) { Y=I=Q=0; }
                Palette.insert(Color(A,Y,I,Q));  // alpha first so sorting makes more sense
                if (Palette.size() > MAX_PALETTE_SIZE) return false;
            }
        }
        for (Color c : Palette) Palette_vector.push_back(c);
//        printf("Palette size: %lu\n",Palette.size());
        return true;
    }
    void data(Image& image) const {
//        printf("TransformPalette::data\n");
        for (int r=0; r<image.rows(); r++) {
            for (int c=0; c<image.cols(); c++) {
                Color C(image(3,r,c), image(0,r,c), image(1,r,c), image(2,r,c));
                if (std::get<0>(C) == 0) { std::get<1>(C) = std::get<2>(C) = std::get<3>(C) = 0; }
                ColorVal P=0;
                for (Color c : Palette_vector) {if (c==C) break; else P++;}
                image(0,r,c) = P;
                image(1,r,c) = 0;
                image(2,r,c) = 0;
                image(3,r,c) = 1;
            }
        }
    }
    void invData(Image& image) const {
        for (int r=0; r<image.rows(); r++) {
            for (int c=0; c<image.cols(); c++) {
                int P=image(0,r,c);
                image(0,r,c) = std::get<1>(Palette_vector[P]);
                image(1,r,c) = std::get<2>(Palette_vector[P]);
                image(2,r,c) = std::get<3>(Palette_vector[P]);
                image(3,r,c) = std::get<0>(Palette_vector[P]);
            }
        }
    }
    void save(const ColorRanges *srcRanges, RacOut &rac) const {
        SimpleSymbolCoder<SimpleBitChance, RacOut, 24> coder(rac);
        SimpleSymbolCoder<SimpleBitChance, RacOut, 24> coderY(rac);
        SimpleSymbolCoder<SimpleBitChance, RacOut, 24> coderI(rac);
        SimpleSymbolCoder<SimpleBitChance, RacOut, 24> coderQ(rac);
        SimpleSymbolCoder<SimpleBitChance, RacOut, 24> coderA(rac);
        Color min(srcRanges->min(3), srcRanges->min(0), srcRanges->min(1), srcRanges->min(2));
        Color max(srcRanges->max(3), srcRanges->max(0), srcRanges->max(1), srcRanges->max(2));
        coder.write_int(1, MAX_PALETTE_SIZE, Palette_vector.size());
//        printf("Saving %lu colors: ", Palette_vector.size());
        Color prev(-1,-1,-1,-1);
        prevPlanes pp(3);
        for (Color c : Palette_vector) {
                ColorVal A=std::get<0>(c);
                coderA.write_int(std::get<0>(min), std::get<0>(max), A);
                if (std::get<0>(c) == 0) continue;
                ColorVal Y=std::get<1>(c);
                coderY.write_int((std::get<0>(prev) == A ? std::get<1>(prev) : std::get<1>(min)), std::get<1>(max), Y);
                pp[0]=Y; srcRanges->minmax(1,pp,std::get<2>(min), std::get<2>(max));
                ColorVal I=std::get<2>(c);
                coderI.write_int(std::get<2>(min), std::get<2>(max), I);
                pp[1]=I; srcRanges->minmax(2,pp,std::get<3>(min), std::get<3>(max));
                coderQ.write_int(std::get<3>(min), std::get<3>(max), std::get<3>(c));

                std::get<0>(min) = std::get<0>(c);
                prev = c;
//                printf("YIQ(%i,%i,%i)\t", std::get<0>(c), std::get<1>(c), std::get<2>(c));
        }
//        printf("\nSaved palette of size: %lu\n",Palette_vector.size());
        v_printf(5,"[%lu]",Palette_vector.size());
    }
    void load(const ColorRanges *srcRanges, RacIn &rac) {
        SimpleSymbolCoder<SimpleBitChance, RacIn, 24> coder(rac);
        SimpleSymbolCoder<SimpleBitChance, RacIn, 24> coderY(rac);
        SimpleSymbolCoder<SimpleBitChance, RacIn, 24> coderI(rac);
        SimpleSymbolCoder<SimpleBitChance, RacIn, 24> coderQ(rac);
        SimpleSymbolCoder<SimpleBitChance, RacIn, 24> coderA(rac);
        Color min(srcRanges->min(3), srcRanges->min(0), srcRanges->min(1), srcRanges->min(2));
        Color max(srcRanges->max(3), srcRanges->max(0), srcRanges->max(1), srcRanges->max(2));
        long unsigned size = coder.read_int(1, MAX_PALETTE_SIZE);
//        printf("Loading %lu colors: ", size);
        Color prev(-1,-1,-1,-1);
        prevPlanes pp(3);
        for (unsigned int p=0; p<size; p++) {
                ColorVal A=coderA.read_int(std::get<0>(min), std::get<0>(max));
                if (A == 0) { Palette_vector.push_back(Color(0,0,0,0)); continue; }
                ColorVal Y=coderY.read_int((std::get<0>(prev) == A ? std::get<1>(prev) : std::get<1>(min)), std::get<1>(max));
                pp[0]=Y; srcRanges->minmax(1,pp,std::get<2>(min), std::get<2>(max));
                ColorVal I=coderI.read_int(std::get<2>(min), std::get<2>(max));
                pp[1]=I; srcRanges->minmax(2,pp,std::get<3>(min), std::get<3>(max));
                ColorVal Q=coderQ.read_int(std::get<3>(min), std::get<3>(max));
                Color c(A,Y,I,Q);
                Palette_vector.push_back(c);
                std::get<0>(min) = std::get<0>(c);
                prev = c;
//                printf("YIQ(%i,%i,%i)\t", std::get<0>(c), std::get<1>(c), std::get<2>(c));
        }
//        printf("\nLoaded palette of size: %lu\n",Palette_vector.size());
        v_printf(5,"[%lu]",Palette_vector.size());
    }
};


#endif
