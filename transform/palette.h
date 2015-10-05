#ifndef FLIF_PALETTE_H
#define FLIF_PALETTE_H 1

#include "../image/image.h"
#include "../image/color_range.h"
#include "transform.h"
#include <tuple>
#include <set>

#define MAX_PALETTE_SIZE 30000

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


template <typename IO>
class TransformPalette : public Transform<IO> {
protected:
    typedef std::tuple<ColorVal,ColorVal,ColorVal> Color;
    std::set<Color> Palette;
    std::vector<Color> Palette_vector;
    unsigned int max_palette_size;

public:
    void configure(const int setting) { max_palette_size = setting;}
    bool init(const ColorRanges *srcRanges) {
        if (srcRanges->numPlanes() < 3) return false;
        if (srcRanges->min(0) < 0 || srcRanges->min(1) < 0 || srcRanges->min(2) < 0) return false;
        if (srcRanges->min(1) == srcRanges->max(1)) return false; // probably grayscale/monochrome, better not use palette then
        if (srcRanges->min(2) == srcRanges->max(2)) return false;
        return true;
    }

    const ColorRanges *meta(Images& images, const ColorRanges *srcRanges) {
        for (Image& image : images) image.palette=true;
        return new ColorRangesPalette(srcRanges, Palette_vector.size());
    }

    bool process(const ColorRanges *srcRanges, const Images &images) {
        for (const Image& image : images)
        for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                int Y=image(0,r,c), I=image(1,r,c), Q=image(2,r,c);
                Palette.insert(Color(Y,I,Q));
                if (Palette.size() > max_palette_size) return false;
            }
        }
        for (Color c : Palette) Palette_vector.push_back(c);
//        printf("Palette size: %lu\n",Palette.size());
        return true;
    }
    void data(Images& images) const {
//        printf("TransformPalette::data\n");
        for (Image& image : images) {
          for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                Color C(image(0,r,c), image(1,r,c), image(2,r,c));
                ColorVal P=0;
                for (Color c : Palette_vector) {if (c==C) break; else P++;}
                image.set(0,r,c, P);
                image.set(1,r,c, 0);
                image.set(2,r,c, 0);
            }
          }
        }
    }
    void invData(Images& images) const {
        for (Image& image : images) {
          for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                int P=image(0,r,c);
                image.set(0,r,c, std::get<0>(Palette_vector[P]));
                image.set(1,r,c, std::get<1>(Palette_vector[P]));
                image.set(2,r,c, std::get<2>(Palette_vector[P]));
            }
          }
          image.palette=false;
        }
    }
    void save(const ColorRanges *srcRanges, RacOut<IO> &rac) const {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 24> coder(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 24> coderY(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 24> coderI(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 24> coderQ(rac);
        Color min(srcRanges->min(0), srcRanges->min(1), srcRanges->min(2));
        Color max(srcRanges->max(0), srcRanges->max(1), srcRanges->max(2));
        coder.write_int(1, MAX_PALETTE_SIZE, Palette_vector.size());
//        printf("Saving %lu colors: ", Palette_vector.size());
        Color prev(-1,-1,-1);
        prevPlanes pp(2);
        for (Color c : Palette_vector) {
                ColorVal Y=std::get<0>(c);
                coderY.write_int(std::get<0>(min), std::get<0>(max), Y);
                pp[0]=Y; srcRanges->minmax(1,pp,std::get<1>(min), std::get<1>(max));
                ColorVal I=std::get<1>(c);
                coderI.write_int((std::get<0>(prev) == Y ? std::get<1>(prev): std::get<1>(min)), std::get<1>(max), I);
                pp[1]=I; srcRanges->minmax(2,pp,std::get<2>(min), std::get<2>(max));
                coderQ.write_int(std::get<2>(min), std::get<2>(max), std::get<2>(c));
                std::get<0>(min) = std::get<0>(c);
                prev = c;
//                printf("YIQ(%i,%i,%i)\t", std::get<0>(c), std::get<1>(c), std::get<2>(c));
        }
//        printf("\nSaved palette of size: %lu\n",Palette_vector.size());
        v_printf(5,"[%lu]",Palette_vector.size());
    }
    void load(const ColorRanges *srcRanges, RacIn<IO> &rac) {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 24> coder(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 24> coderY(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 24> coderI(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 24> coderQ(rac);
        Color min(srcRanges->min(0), srcRanges->min(1), srcRanges->min(2));
        Color max(srcRanges->max(0), srcRanges->max(1), srcRanges->max(2));
        long unsigned size = coder.read_int(1, MAX_PALETTE_SIZE);
//        printf("Loading %lu colors: ", size);
        Color prev(-1,-1,-1);
        prevPlanes pp(2);
        for (unsigned int p=0; p<size; p++) {
                ColorVal Y=coderY.read_int(std::get<0>(min), std::get<0>(max));
                pp[0]=Y; srcRanges->minmax(1,pp,std::get<1>(min), std::get<1>(max));
                ColorVal I=coderI.read_int((std::get<0>(prev) == Y ? std::get<1>(prev): std::get<1>(min)), std::get<1>(max));
                pp[1]=I; srcRanges->minmax(2,pp,std::get<2>(min), std::get<2>(max));
                ColorVal Q=coderQ.read_int(std::get<2>(min), std::get<2>(max));
                Color c(Y,I,Q);
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
