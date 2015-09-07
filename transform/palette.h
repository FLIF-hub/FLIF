#ifndef _PALETTE_H_
#define _PALETTE_H_ 1

#include "../image/image.h"
#include "../image/color_range.h"
#include "transform.h"
#include <tuple>
#include <set>

#define MAX_PALETTE_SIZE 512

class ColorRangesPalette : public ColorRanges
{
protected:
    const ColorRanges *ranges;
    int nb_colors;
public:
    ColorRangesPalette(const ColorRanges *rangesIn, const int nb) : ranges(rangesIn), nb_colors(nb) { }
    bool isStatic() const { return false; }
    int numPlanes() const { return ranges->numPlanes(); }

    ColorVal min(int p) const { if (p<3) return 0; else return ranges->min(p); }
    ColorVal max(int p) const { switch(p) {
                                        case 0: return nb_colors-1;
                                        case 1: return 0;
                                        case 2: return 0;
                                        default: return ranges->max(p);
                                         };
                              }
    void minmax(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv) const {
         if (p==0) { minv=0; maxv=nb_colors-1; return;}
         else if (p<3) { minv=0; maxv=0; return;}
         else ranges->minmax(p,pp,minv,maxv);
    }

};


class TransformPalette : public Transform {
protected:
    typedef std::tuple<ColorVal,ColorVal,ColorVal> Color;
    std::set<Color> Palette;
    std::vector<Color> Palette_vector;

public:
    bool virtual init(const ColorRanges *srcRanges) {
        if (srcRanges->numPlanes() < 3) return false;
        if (srcRanges->min(0) < 0 || srcRanges->min(1) < 0 || srcRanges->min(2) < 0) return false;
        if (srcRanges->min(1) == srcRanges->max(1)) return false; // probably grayscale/monochrome, better not use palette then
        if (srcRanges->min(2) == srcRanges->max(2)) return false;
        return true;
    }

    const ColorRanges *meta(Image& image, const ColorRanges *srcRanges) {
        return new ColorRangesPalette(srcRanges, Palette_vector.size());
    }

    bool process(const ColorRanges *srcRanges, const Image &image) {
        for (int r=0; r<image.rows(); r++) {
            for (int c=0; c<image.cols(); c++) {
                int Y=image(0,r,c), I=image(1,r,c), Q=image(2,r,c);
                Palette.insert(Color(Y,I,Q));
                if (Palette.size() > MAX_PALETTE_SIZE) return false;
            }
        }
        for (Color c : Palette) Palette_vector.push_back(c);
        printf("Palette size: %lu\n",Palette.size());
        return true;
    }
    void data(Image& image) const {
        printf("TransformPalette::data\n");
        for (int r=0; r<image.rows(); r++) {
            for (int c=0; c<image.cols(); c++) {
                Color C(image(0,r,c), image(1,r,c), image(2,r,c));
                ColorVal P=0;
                for (Color c : Palette_vector) {if (c==C) break; else P++;}
                image(0,r,c) = P;
                image(1,r,c) = 0;
                image(2,r,c) = 0;
            }
        }
    }
    void invData(Image& image) const {
        for (int r=0; r<image.rows(); r++) {
            for (int c=0; c<image.cols(); c++) {
                int P=image(0,r,c);
                image(0,r,c) = std::get<0>(Palette_vector[P]);
                image(1,r,c) = std::get<1>(Palette_vector[P]);
                image(2,r,c) = std::get<2>(Palette_vector[P]);
            }
        }
    }
    void save(const ColorRanges *srcRanges, RacOut &rac) const {
        SimpleSymbolCoder<StaticBitChance, RacOut, 24> coder(rac);
        Color min(srcRanges->min(0), srcRanges->min(1), srcRanges->min(2));
        Color max(srcRanges->max(0), srcRanges->max(1), srcRanges->max(2));
        coder.write_int(1, MAX_PALETTE_SIZE, Palette_vector.size());
        printf("Saving %lu colors: ", Palette_vector.size());
        for (Color c : Palette_vector) {
                coder.write_int(std::get<0>(min), std::get<0>(max), std::get<0>(c));
                coder.write_int(std::get<1>(min), std::get<1>(max), std::get<1>(c));
                coder.write_int(std::get<2>(min), std::get<2>(max), std::get<2>(c));
                std::get<0>(min) = std::get<0>(c);
                printf("YIQ(%i,%i,%i)\t", std::get<0>(c), std::get<1>(c), std::get<2>(c));
        }
        printf("\nSaved palette of size: %lu\n",Palette_vector.size());
    }
    void load(const ColorRanges *srcRanges, RacIn &rac) {
        SimpleSymbolCoder<StaticBitChance, RacIn, 24> coder(rac);
        Color min(srcRanges->min(0), srcRanges->min(1), srcRanges->min(2));
        Color max(srcRanges->max(0), srcRanges->max(1), srcRanges->max(2));
        long unsigned size = coder.read_int(1, MAX_PALETTE_SIZE);
        printf("Loading %lu colors: ", size);
        for (unsigned int p=0; p<size; p++) {
                ColorVal Y=coder.read_int(std::get<0>(min), std::get<0>(max));
                ColorVal I=coder.read_int(std::get<1>(min), std::get<1>(max));
                ColorVal Q=coder.read_int(std::get<2>(min), std::get<2>(max));
                Color c(Y,I,Q);
                Palette_vector.push_back(c);
                std::get<0>(min) = std::get<0>(c);
                printf("YIQ(%i,%i,%i)\t", std::get<0>(c), std::get<1>(c), std::get<2>(c));
        }
        printf("\nLoaded palette of size: %lu\n",Palette_vector.size());
    }
};


#endif
