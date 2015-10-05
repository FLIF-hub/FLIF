#ifndef FLIF_FRAMECOMBINE_H
#define FLIF_FRAMECOMBINE_H 1

#include <vector>

#include "transform.h"


class ColorRangesFC : public ColorRanges
{
protected:
    ColorVal negnumPrevFrames;
    ColorVal alpha_max;
    const ColorRanges *ranges;
public:
    ColorRangesFC(const ColorVal amin, const ColorVal amax, const ColorRanges *rangesIn) : negnumPrevFrames(amin), alpha_max(amax), ranges(rangesIn) {}
    bool isStatic() const { return false; }
    int numPlanes() const { return 4; }
    ColorVal min(int p) const { if (p<3) return ranges->min(p); else return negnumPrevFrames; }
    ColorVal max(int p) const { if (p<3) return ranges->max(p); else return alpha_max;}
    void minmax(const int p, const prevPlanes &pp, ColorVal &min, ColorVal &max) const {
        if (p == 3) { min=negnumPrevFrames; max=alpha_max; }
        else ranges->minmax(p, pp, min, max);
    }
};

template <typename IO>
class TransformFrameCombine : public Transform<IO> {
protected:
    bool was_flat;
    int max_lookback;
    int user_max_lookback;
    const ColorRanges *meta(Images& images, const ColorRanges *srcRanges) {
        was_flat = srcRanges->numPlanes() < 4;
        if (was_flat)
          for (unsigned int fr=0; fr<images.size(); fr++) {
            Image& image = images[fr];
            image.ensure_alpha();
          }
        int lookback = 1-(int)images.size();
        if (lookback < -max_lookback) lookback=-max_lookback;
        return new ColorRangesFC(lookback, (srcRanges->numPlanes() == 4 ? srcRanges->max(3) : 1), srcRanges);
    }

    void load(const ColorRanges *srcRanges, RacIn<IO> &rac) {
        SimpleSymbolCoder<SimpleBitChance, RacIn<IO>, 24> coder(rac);
        max_lookback = coder.read_int(1, 256);
        v_printf(5,"[%i]",max_lookback);
    }

    void save(const ColorRanges *srcRanges, RacOut<IO> &rac) const {
        SimpleSymbolCoder<SimpleBitChance, RacOut<IO>, 24> coder(rac);
        coder.write_int(1,256,max_lookback);
    }

// a heuristic to figure out if this is going to help (it won't help if we introduce more entropy than what is eliminated)
    bool process(const ColorRanges *srcRanges, const Images &images) {
        if (images.size() < 2) return false;
        int nump=images[0].numPlanes();
        int pixel_cost = 1;
        for (int p=0; p<nump; p++) pixel_cost *= (1 + srcRanges->max(p) - srcRanges->min(p));
        // pixel_cost is roughly the cost per pixel (number of different values a pixel can take)
        if (pixel_cost < 16) return false; // pixels are too cheap, no point in trying to save stuff
        std::vector<uint64_t> found_pixels(images.size(), 0);
        uint64_t new_pixels=0;
        max_lookback=1;
        if (user_max_lookback == -1) user_max_lookback = images.size()-1;
        for (int fr=1; fr < (int)images.size(); fr++) {
            const Image& image = images[fr];
            for (uint32_t r=0; r<image.rows(); r++) {
                for (uint32_t c=image.col_begin[r]; c<image.col_end[r]; c++) {
                    new_pixels++;
                    if (nump>3 && image(3,r,c) == 0) { continue; }
                    for (int prev=1; prev <= fr; prev++) {
                        if (prev>user_max_lookback) break;
                        bool identical=true;
                        for (int p=0; p<nump; p++) {
                          if(image(p,r,c) != images[fr-prev](p,r,c)) { identical=false; break;}
                        }
                        if (identical) { found_pixels[prev]++; new_pixels--; if (prev>max_lookback) max_lookback=prev; break;}
                    }
                }
            }
        }
        if (images.size() > 2) v_printf(7,", trying_FRA(at -1: %llu, at -2: %llu, new: %llu)",(long long unsigned) found_pixels[1],(long long unsigned) found_pixels[2], (long long unsigned) new_pixels);
        if (max_lookback>256) max_lookback=256;
        for(int i=1; i <= max_lookback; i++) {
            v_printf(8,"at lookback %i: %llu pixels\n",-i, found_pixels[i]);
            if (found_pixels[i] <= new_pixels/200 || i>pixel_cost) {max_lookback=i-1; break;}
            found_pixels[0] += found_pixels[i];
        }
        for(int i=max_lookback+1; i<(int)images.size(); i++) {
            if (found_pixels[i] > new_pixels/200 && i<pixel_cost) {max_lookback=i; found_pixels[0] += found_pixels[i];}
            else new_pixels += found_pixels[i];
        }

        return (found_pixels[0] * pixel_cost > new_pixels * (2 + max_lookback));
    };

    void configure(int setting) { user_max_lookback=setting; }
    void data(Images &images) const {
        for (int fr=1; fr < (int)images.size(); fr++) {
            uint32_t ipixels=0;
            Image& image = images[fr];
            for (uint32_t r=0; r<image.rows(); r++) {
                for (uint32_t c=image.col_begin[r]; c<image.col_end[r]; c++) {
                    if (image(3,r,c) == 0) continue;
                    for (int prev=1; prev <= fr; prev++) {
                        if (prev>max_lookback) break;
                        bool identical=true;
                        for (int p=0; p<3; p++) {
                          if(image(p,r,c) != images[fr-prev](p,r,c)) { identical=false; break;}
                        }
                        if (images[fr-prev](3,r,c) < 0) {
                          if (image(3,r,c) != images[fr-prev+images[fr-prev](3,r,c)](3,r,c)) identical=false;
                        } else {
                          if (image(3,r,c) != images[fr-prev](3,r,c)) identical=false;
                        }
                        if (identical) {image.set(3,r,c, -prev); ipixels++; break;}
                    }
                }
            }
//            printf("frame %i: found %u pixels from previous frames\n", fr, ipixels);
        }
    }
    void invData(Images &images) const {
        // has to be done on the fly for all channels except A
        for (int fr=1; fr<(int)images.size(); fr++) {
            uint32_t ipixels=0;
            Image& image = images[fr];
            for (uint32_t r=0; r<image.rows(); r++) {
                for (uint32_t c=image.col_begin[r]; c<image.col_end[r]; c++) {
                    if (image(3,r,c) >= 0) continue;
                    assert(fr+image(3,r,c) >= 0);
                    image.set(3,r,c, images[fr+image(3,r,c)](3,r,c));
                    ipixels++;
                }
            }
//            printf("frame %i: found %u pixels from previous frames\n", fr, ipixels);
        }
        if (was_flat) for (Image& image : images) image.drop_alpha();
    }
};

#endif
