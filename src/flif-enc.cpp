/*
 FLIF encoder - Free Lossless Image Format
 Copyright (C) 2010-2016  Jon Sneyers & Pieter Wuille, LGPL v3+

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"
#ifdef HAS_ENCODER
#include <string>
#include <string.h>

#include "maniac/rac.hpp"
#include "maniac/compound.hpp"
#include "maniac/util.hpp"

#include "flif_config.h"
#include "image/color_range.hpp"
#include "transform/factory.hpp"


#include "common.hpp"
#include "fileio.hpp"

using namespace maniac::util;

template<typename RAC> void static write_name(RAC& rac, std::string desc) {
    int nb = 0;
    while (nb <= MAX_TRANSFORM) {
        if (transforms[nb] == desc) break;
        nb++;
    }
    if (transforms[nb] != desc) { e_printf("ERROR: Invalid transform: '%s'\n",desc.c_str()); return;}
    UniformSymbolCoder<RAC> coder(rac);
    coder.write_int(0, MAX_TRANSFORM, nb);
}

// alphazero = true: image has alpha plane and A=0 implies RGB(YIQ) is irrelevant
// alphazero = false: image either has no alpha plane, or A=0 has no special meaning
// FRA = true: image has FRA plane (animation with lookback)
template<typename IO, typename Rac, typename Coder>
void flif_encode_scanlines_inner(IO& io, Rac& rac, std::vector<Coder> &coders, const Images &images, const ColorRanges *ranges) {
    const std::vector<ColorVal> greys = computeGreys(ranges);
    ColorVal min,max;
    long fs = io.ftell();
    long pixels = images[0].cols()*images[0].rows()*images.size();
    const int nump = images[0].numPlanes();
    const bool alphazero = (nump>3 && images[0].alpha_zero_special);
#ifdef SUPPORT_ANIMATION
    const bool FRA = (nump == 5);
#endif
    for (int k=0,i=0; k < 5; k++) {
        int p=PLANE_ORDERING[k];
        if (p>=nump) continue;
        i++;
        if (ranges->min(p) >= ranges->max(p)) continue;
        const ColorVal minP = ranges->min(p);
        Properties properties((nump>3?NB_PROPERTIES_scanlinesA[p]:NB_PROPERTIES_scanlines[p]));
        v_printf_tty(2,"\r%i%% done [%i/%i] ENC[%ux%u]    ",(int)(100*pixels_done/pixels_todo),i,nump,images[0].cols(),images[0].rows());
        pixels_done += images[0].cols()*images[0].rows();
        for (uint32_t r = 0; r < images[0].rows(); r++) {
            for (int fr=0; fr< (int)images.size(); fr++) {
              const Image& image = images[fr];
              if (image.seen_before >= 0) continue;
              uint32_t begin=image.col_begin[r], end=image.col_end[r];
              for (uint32_t c = begin; c < end; c++) {
                if (alphazero && p<3 && image(3,r,c) == 0) continue;
#ifdef SUPPORT_ANIMATION
                if (FRA && p<4 && image(4,r,c) > 0) continue;
#endif
                ColorVal guess = predict_and_calcProps_scanlines(properties,ranges,image,p,r,c,min,max, minP);
                ColorVal curr = image(p,r,c);
                assert(p != 3 || curr >= -fr);
#ifdef SUPPORT_ANIMATION
                if (FRA && p==4 && max > fr) max = fr;
#endif
                coders[p].write_int(properties, min - guess, max - guess, curr - guess);
              }
            }
        }
        long nfs = io.ftell();
        if (nfs-fs > 0) {
           v_printf(3,"filesize : %li (+%li for %li pixels, %f bpp)", nfs, nfs-fs, pixels, 8.0*(nfs-fs)/pixels );
           v_printf(4,"\n");
        }
        fs = nfs;
    }
}

template<typename IO, typename Rac, typename Coder>
void flif_encode_scanlines_pass(IO& io, Rac &rac, const Images &images, const ColorRanges *ranges, std::vector<Tree> &forest, int repeats, flif_options &options) {

    std::vector<Coder> coders;
    coders.reserve(ranges->numPlanes());

    for (int p = 0; p < ranges->numPlanes(); p++) {
        Ranges propRanges;
        initPropRanges_scanlines(propRanges, *ranges, p);
        coders.emplace_back(rac, propRanges, forest[p], options.split_threshold, options.cutoff, options.alpha);
    }

    while(repeats-- > 0) {
     flif_encode_scanlines_inner<IO,Rac,Coder>(io, rac, coders, images, ranges);
    }

    for (int p = 0; p < ranges->numPlanes(); p++) {
        coders[p].simplify(options.divisor, options.min_size, p);
    }
}

// return the predictor that has the smallest total difference
int find_best_predictor(const Images &images, const ColorRanges *ranges, const int p, const int z) {
    const int zerobonus = 1;
    ColorVal min,max;
    const int nump = images[0].numPlanes();
    const bool alphazero = (nump>3 && images[0].alpha_zero_special);
#ifdef SUPPORT_ANIMATION
    const bool FRA = (nump == 5);
#endif
    Properties properties((nump>3?NB_PROPERTIESA[p]:NB_PROPERTIES[p]));
    std::vector<uint64_t> total_size(MAX_PREDICTOR+1, 0);
    for (int predictor=0; predictor <= MAX_PREDICTOR; predictor++) {
      //Ranges propRanges;
      //initPropRanges(propRanges, *ranges, p);
      //RacDummy dummy;
      //Tree tree;
      //PropertySymbolCoder<FLIFBitChancePass1, RacDummy, 18> coder(dummy, propRanges, tree); //, split_threshold, cutoff, alpha);
      if (z % 2 == 0) {
        // horizontal: scan the odd rows, output pixel values
          for (uint32_t r = 1; r < images[0].rows(z); r += 2) {
            for (int fr=0; fr<(int)images.size(); fr++) {
              const Image& image = images[fr];
              if (image.seen_before >= 0) { continue; }
              uint32_t begin=(image.col_begin[r*image.zoom_rowpixelsize(z)]/image.zoom_colpixelsize(z)),
                         end=(1+(image.col_end[r*image.zoom_rowpixelsize(z)]-1)/image.zoom_colpixelsize(z));
              for (uint32_t c = begin; c < end; c++) {
                    if (alphazero && p<3 && image(3,z,r,c) == 0) continue;
#ifdef SUPPORT_ANIMATION
                    if (FRA && p<4 && image(4,z,r,c) > 0) continue;
#endif
                    ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max,predictor);
                    ColorVal curr = image(p,z,r,c);
                    total_size[predictor] += maniac::util::ilog2(abs(curr-guess)) + (curr-guess ? zerobonus : 0);
                    //if (FRA && p==4 && max > fr) max = fr;
                    //coder.write_int(properties, min - guess, max - guess, curr - guess);
              }
            }
          }
      } else {
        // vertical: scan the odd columns
          for (uint32_t r = 0; r < images[0].rows(z); r++) {
            for (int fr=0; fr<(int)images.size(); fr++) {
              const Image& image = images[fr];
              if (image.seen_before >= 0) { continue; }
              uint32_t begin=(image.col_begin[r*image.zoom_rowpixelsize(z)]/image.zoom_colpixelsize(z)),
                         end=(1+(image.col_end[r*image.zoom_rowpixelsize(z)]-1)/image.zoom_colpixelsize(z))|1;
              if (begin>1 && ((begin&1) ==0)) begin--;
              if (begin==0) begin=1;
              for (uint32_t c = begin; c < end; c+=2) {
                    if (alphazero && p<3 && image(3,z,r,c) == 0) continue;
#ifdef SUPPORT_ANIMATION
                    if (FRA && p<4 && image(4,z,r,c) > 0) continue;
#endif
                    ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max,predictor);
                    ColorVal curr = image(p,z,r,c);
                    total_size[predictor] += maniac::util::ilog2(abs(curr-guess)) + (curr-guess ? zerobonus : 0);
                    //if (FRA && p==4 && max > fr) max = fr;
                    //coder.write_int(properties, min - guess, max - guess, curr - guess);
              }
            }
          }
      }
      //total_size[predictor] = coder.compute_total_size();
    }
    int best = 0;
//    total_size[0] = 9*total_size[0]/10; // give an advantage to predictor 0, because if it's a close race, then 0 is usually better in the end
    for (int predictor=0; predictor <= MAX_PREDICTOR; predictor++)
        if (total_size[predictor] < total_size[best])
            best=predictor;
    return best;
}

template<typename IO, typename Rac, typename Coder>
void flif_encode_FLIF2_inner(IO& io, Rac& rac, std::vector<Coder> &coders, const Images &images,
                             const ColorRanges *ranges, const int beginZL, const int endZL, flif_options &options) {
    ColorVal min,max;
    const int nump = images[0].numPlanes();
    const bool alphazero = (nump>3 && images[0].alpha_zero_special);
    const int *the_predictor = options.predictor;
#ifdef SUPPORT_ANIMATION
    const bool FRA = (nump == 5);
#endif
    long fs = io.ftell();
    UniformSymbolCoder<Rac> metaCoder(rac);
    const bool default_order = (options.chroma_subsampling==0);
    metaCoder.write_int(0, 1, (default_order? 1 : 0)); // we're using the default zoomlevel/plane ordering
    for (int p=0; p<nump; p++) metaCoder.write_int(-1, MAX_PREDICTOR, the_predictor[p]);
    for (int i = 0; i < plane_zoomlevels(images[0], beginZL, endZL); i++) {
      std::pair<int, int> pzl = plane_zoomlevel(images[0], beginZL, endZL, i, ranges);
      int p = pzl.first;
      int z = pzl.second;
      if (options.chroma_subsampling && p > 0 && p < 3 && z < 2) continue;
      if (!default_order) metaCoder.write_int(0, nump-1, p);
      if (ranges->min(p) >= ranges->max(p)) continue;
      int predictor = (the_predictor[p] < 0 ? find_best_predictor(images, ranges, p, z) : the_predictor[p]);
      //if (z < 2 && the_predictor < 0) printf("Plane %i, zoomlevel %i: predictor %i\n",p,z,predictor);
      if (the_predictor[p] < 0) metaCoder.write_int(0, MAX_PREDICTOR, predictor);
      if (endZL == 0) v_printf_tty(2,"\r%i%% done [%i/%i] ENC[%i,%ux%u]  ",(int)(100*pixels_done/pixels_todo),i,plane_zoomlevels(images[0], beginZL, endZL)-1,p,images[0].cols(z),images[0].rows(z));
      Properties properties((nump>3?NB_PROPERTIESA[p]:NB_PROPERTIES[p]));
      if (z % 2 == 0) {
        // horizontal: scan the odd rows, output pixel values
          for (uint32_t r = 1; r < images[0].rows(z); r += 2) {
            pixels_done += images[0].cols(z);
            if (endZL == 0 && (r & 257)==257) v_printf_tty(3,"\r%i%% done [%i/%i] ENC[%i,%ux%u]  ",(int)(100*pixels_done/pixels_todo),i,plane_zoomlevels(images[0], beginZL, endZL)-1,p,images[0].cols(z),images[0].rows(z));
            for (int fr=0; fr<(int)images.size(); fr++) {
              const Image& image = images[fr];
              if (image.seen_before >= 0) { continue; }
              uint32_t begin=(image.col_begin[r*image.zoom_rowpixelsize(z)]/image.zoom_colpixelsize(z)),
                         end=(1+(image.col_end[r*image.zoom_rowpixelsize(z)]-1)/image.zoom_colpixelsize(z));
              for (uint32_t c = begin; c < end; c++) {
                    if (alphazero && p<3 && image(3,z,r,c) == 0) continue;
#ifdef SUPPORT_ANIMATION
                    if (FRA && p<4 && image(4,z,r,c) > 0) continue;
#endif
                    ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max,predictor);
                    ColorVal curr = image(p,z,r,c);
#ifdef SUPPORT_ANIMATION
                    if (FRA) {
                        if (p==4 && max > fr) max = fr;
                        if (guess>max || guess<min) guess = min;
                    }
#endif
                    assert (curr <= max); assert (curr >= min);
                    coders[p].write_int(properties, min - guess, max - guess, curr - guess);
              }
            }
          }
      } else {
        // vertical: scan the odd columns
          for (uint32_t r = 0; r < images[0].rows(z); r++) {
            pixels_done += images[0].cols(z)/2;
            if (endZL == 0 && (r&513)==513) v_printf_tty(3,"\r%i%% done [%i/%i] ENC[%i,%ux%u]  ",(int)(100*pixels_done/pixels_todo),i,plane_zoomlevels(images[0], beginZL, endZL)-1,p,images[0].cols(z),images[0].rows(z));
            for (int fr=0; fr<(int)images.size(); fr++) {
              const Image& image = images[fr];
              if (image.seen_before >= 0) { continue; }
              uint32_t begin=(image.col_begin[r*image.zoom_rowpixelsize(z)]/image.zoom_colpixelsize(z)),
                         end=(1+(image.col_end[r*image.zoom_rowpixelsize(z)]-1)/image.zoom_colpixelsize(z))|1;
              if (begin>1 && ((begin&1) ==0)) begin--;
              if (begin==0) begin=1;
              for (uint32_t c = begin; c < end; c+=2) {
                    if (alphazero && p<3 && image(3,z,r,c) == 0) continue;
#ifdef SUPPORT_ANIMATION
                    if (FRA && p<4 && image(4,z,r,c) > 0) continue;
#endif
                    ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max,predictor);
                    ColorVal curr = image(p,z,r,c);
#ifdef SUPPORT_ANIMATION
                    if (FRA) {
                        if (p==4 && max > fr) max = fr;
                        if (guess>max || guess<min) guess = min;
                    }
#endif
                    assert (curr <= max); assert (curr >= min);
                    coders[p].write_int(properties, min - guess, max - guess, curr - guess);
              }
            }
          }
      }
      if (endZL==0 && io.ftell()>fs) {
          v_printf_tty(3,"    wrote %li bytes    ", io.ftell());
          v_printf_tty(5,"\n");
          fs = io.ftell();
      }
    }
    if (options.chroma_subsampling && nump>1 && endZL==0) metaCoder.write_int(0, nump-1, 1); // pretend to be interrupted right after Co zoomlevel 1 started
}

template<typename IO, typename Rac, typename Coder>
void flif_encode_FLIF2_pass(IO& io, Rac &rac, const Images &images, const ColorRanges *ranges, std::vector<Tree> &forest, const int beginZL, const int endZL, int repeats, flif_options &options) {
    std::vector<Coder> coders;
    coders.reserve(ranges->numPlanes());
    for (int p = 0; p < ranges->numPlanes(); p++) {
        Ranges propRanges;
        initPropRanges(propRanges, *ranges, p);
        coders.emplace_back(rac, propRanges, forest[p], options.split_threshold, options.cutoff, options.alpha);
    }

    if (beginZL == images[0].zooms() && endZL>0) {
      // special case: very left top pixel must be written first to get it all started
//      SimpleSymbolCoder<FLIFBitChanceMeta, Rac, 18> metaCoder(rac);
      UniformSymbolCoder<Rac> metaCoder(rac);
      for (int p = 0; p < images[0].numPlanes(); p++) {
        if (ranges->min(p) < ranges->max(p)) {
            for (const Image& image : images) metaCoder.write_int(ranges->min(p), ranges->max(p), image(p,0,0,0));
            pixels_done++;
        }
      }
    }
    while(repeats-- > 0) {
     flif_encode_FLIF2_inner<IO,Rac,Coder>(io, rac, coders, images, ranges, beginZL, endZL, options);
    }
    for (int p = 0; p < images[0].numPlanes(); p++) {
        coders[p].simplify(options.divisor, options.min_size, p);
    }
}

void flif_encode_FLIF2_interpol_zero_alpha(Images &images, const ColorRanges * ranges, const int beginZL, const int endZL, const int predictor)
{
    const std::vector<ColorVal> greys = computeGreys(ranges);

    for (Image& image : images) {
     if (image(3,0,0) == 0) {
       image.set(0,0,0,greys[0]);
       image.set(1,0,0,greys[1]);
       image.set(2,0,0,greys[2]);
     }
     for (int i = 0; i < plane_zoomlevels(image, beginZL, endZL); i++) {
      std::pair<int, int> pzl = plane_zoomlevel(image, beginZL, endZL, i, ranges);
      int p = pzl.first;
      int z = pzl.second;
      if (p >= 3) continue;
//      v_printf(2,"[%i] interpol_zero_alpha ",p);
//      fflush(stdout);
      if (z % 2 == 0) {
        // horizontal: scan the odd rows
          for (uint32_t r = 1; r < image.rows(z); r += 2) {
            for (uint32_t c = 0; c < image.cols(z); c++) {
               if (image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c,predictor));
            }
          }
      } else {
        // vertical: scan the odd columns
          for (uint32_t r = 0; r < image.rows(z); r++) {
            for (uint32_t c = 1; c < image.cols(z); c += 2) {
               if (image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c,predictor));
            }
          }
      }
     }
    }
//    v_printf(2,"\n");
}

void flif_encode_scanlines_interpol_zero_alpha(Images &images, const ColorRanges *ranges){
    const std::vector<ColorVal> greys = computeGreys(ranges);

    int nump = images[0].numPlanes();
    if (nump > 3)
    for (Image& image : images)
    for (int p = 0; p < 3; p++) {
//        if (ranges->min(p) >= ranges->max(p)) continue;  // nope, gives problem with fully A=0 image
        for (uint32_t r = 0; r < image.rows(); r++) {
            for (uint32_t c = 0; c < image.cols(); c++) {
                if (image(3,r,c) == 0) {
                    image.set(p,r,c, predictScanlines(image,p,r,c, greys[p]));
                }
            }
        }
    }
}

// Make value (which is a difference from prediction) lossy by rounding small values to zero
// and (for larger values) by setting least significant mantissa bits to zero
ColorVal flif_make_lossy(int min, int max, ColorVal value, int loss) {
    if (loss <= 0) return value;

    if (min == max) return min;
    if (value == 0) return 0;

    const int sign = (value > 0 ? 1 : 0);
    if (sign) { if (min <= 0) min = 1; }
    else { if(max >= 0) max = -1; }
    int a = abs(value);
    if (a < loss) return 0;
    // we are reducing precision on the mantissa by dropping least significant bits, so
    // we round things so the significant bits we keep are as close as possible to the actual value
    int ignoredbits = maniac::util::ilog2(loss);
    a += (1 << ignoredbits) - 1;


    const int e = maniac::util::ilog2(a);
    int amin = sign ? abs(min) : abs(max);
    int amax = sign ? abs(max) : abs(min);

    int have = (1 << e);
    int left = have-1;
    for (int pos = e; pos>0;) {
        int bit = 1;
        left ^= (1 << (--pos));
        int minabs1 = have | (1<<pos);
        int maxabs0 = have | left;
        if (minabs1 > amax) { // 1-bit is impossible
            bit = 0;
        } else if (maxabs0 >= amin) { // 0-bit and 1-bit are both possible
            if (pos > ignoredbits)
                bit = (a >> pos) & 1;
            else
                bit = 0;
        }
        have |= (bit << pos);
    }
    return (sign ? have : -have);

}

void flif_make_lossy_scanlines(Images &images, const ColorRanges *ranges, int loss, bool adaptive, Image &map){
    int nump = images[0].numPlanes();
    const bool alphazero = (nump>3 && images[0].alpha_zero_special);
    const bool FRA = (nump == 5);
    if (nump>4) nump=4; // let's not be lossy on the FRA plane
    int lossp[] = {(loss+6)/10, (loss+5)/7, (loss+5)/6, loss/10, 0};  // less loss on Y, more on Co and Cg
    for (int k=0,i=0; k < 5; k++) {
        int p=PLANE_ORDERING[k];
        if (p>=nump) continue;
        i++;
        if (ranges->min(p) >= ranges->max(p)) continue;
        const ColorVal minP = ranges->min(p);
        ColorVal min, max;
        Properties properties((nump>3?NB_PROPERTIES_scanlinesA[p]:NB_PROPERTIES_scanlines[p]));
        for (uint32_t r = 0; r < images[0].rows(); r++) {
            for (int fr=0; fr< (int)images.size(); fr++) {
              Image& image = images[fr];
              uint32_t begin=image.col_begin[r], end=image.col_end[r];
              for (uint32_t c = begin; c < end; c++) {
                if (adaptive && (map(0,r,c) == 255)) continue;
                if (alphazero && p<3 && image(3,r,c) == 0) continue;
                if (FRA && p<4 && image(4,r,c) > 0) continue;
                ColorVal guess = predict_and_calcProps_scanlines(properties,ranges,image,p,r,c,min,max, minP);
                ColorVal curr = image(p,r,c);
                if (FRA && p==4 && max > fr) max = fr;
                ColorVal diff = flif_make_lossy(min - guess, max - guess, curr - guess,
                                        (adaptive? 255-map(0,r,c) : 255) * lossp[p] / 255);
                ColorVal lossyval = guess+diff;
                ranges->snap(p,properties,min,max,lossyval);
                image.set(p,r,c, lossyval);
              }
            }
        }
    }
}
inline int luma_alpha_compensate(int p, int z, int loss, ColorVal Y, ColorVal X, ColorVal A) {
    if (p==4) return 255;
    if (p==0) return 128+A/2; // divide by 128 at low alpha (so double loss), 255 at high alpha (normal loss)
    if (z>1 || loss > 100 || (abs(X)>32 && Y>64)) return 128+A/2; // chroma: less loss for saturated bright colors
    else return 15+loss/2+Y/4+A/2;                                 // more loss for unsaturated/dark colors (at high quality and final zoomlevels)
}
void flif_make_lossy_interlaced(Images &images, const ColorRanges * ranges, int loss, bool adaptive, Image &map) {
    ColorVal min,max;
    int nump = images[0].numPlanes();
    const bool alphazero = (nump>3 && images[0].alpha_zero_special);
    const bool FRA = (nump == 5);
    int beginZL = images[0].zooms();
    int endZL = 0;
    int predictor=0;
//    int lossp[] = {(loss+6)/10, (loss+2)/4, (loss+2)/3, loss/10, 0};  // less loss on Y, more on Co and Cg
      // Y quality steps:  ..., 77-86, 87-96, 97-100
      // Co quality steps: ..., 83-86, 87-90, 91-94, 95-98, 99-100
      // Cg quality steps: ..., 85-87, 88-90, 91-93, 94-96, 97-99, 100
      // overall quality jumps at: ...., 87, 88, 91, 94, 95, 97, 99, 100

//    int lossp[] = {(loss+6)/10, (loss+1)/3, (loss+2)/3, loss/10, 0};  // less loss on Y, more on Co and Cg
      // Y quality steps:  ..., 77-86, 87-96, 97-100
      // Co quality steps: ..., 87-89, 90-92, 93-95, 96-98, 99-100
      // Cg quality steps: ..., 85-87, 88-90, 91-93, 94-96, 97-99, 100
      // overall quality jumps at: ...., 87, 88, 90, 91, 93, 94, 96, 97, 99, 100

    int lossp[] = {(loss+6)/11, (loss+1)/3, (loss+2)/3, loss/10, 0};  // less loss on Y, more on Co and Cg
      // Y quality steps:  ..., 63-73, 74-84, 85-95, 96-100
      // Co quality steps: ..., 87-89, 90-92, 93-95, 96-98, 99-100
      // Cg quality steps: ..., 85-87, 88-90, 91-93, 94-96, 97-99, 100
      // overall quality jumps at: ...., 87, 88, 89, 90, 91, 93, 94, 96, 97, 98, 99, 100


    // preprocessing step: compensate for anticipated loss in final zoomlevels (assuming predictor 0)
    for (int i = plane_zoomlevels(images[0], beginZL, endZL)-1; i >= 0 ; i--) {
      std::pair<int, int> pzl = plane_zoomlevel(images[0], beginZL, endZL, i, ranges);
      int p = pzl.first;
      int z = pzl.second;
      if (z>7) continue;
      if (loss<100 && z>5) continue;
      if (loss<70 && z>3) continue;
      if (loss<20 && z>1) continue;
      if (ranges->min(p) >= ranges->max(p)) continue;
      Properties properties((nump>3?NB_PROPERTIESA[p]:NB_PROPERTIES[p]));
      if (lossp[p]==0) continue;
//      printf("[%i] p=%i, z=%i\n",i,p,z);
      int factor=255;
      if (z>4) factor = factor * 4 / z;
      assert(beginZL && "division by zero");
      factor = ((beginZL-z)*factor/(beginZL));
      if (z==0) factor += loss*2;
      if (z==1) factor += loss;

      if (z % 2 == 0) {
          for (uint32_t r = 1; r < images[0].rows(z)-1; r += 2) {
            for (int fr=0; fr<(int)images.size(); fr++) {
              Image& image = images[fr];
              if (image.seen_before >= 0) { continue; }
              uint32_t begin=(image.col_begin[r*image.zoom_rowpixelsize(z)]/image.zoom_colpixelsize(z)),
                         end=(1+(image.col_end[r*image.zoom_rowpixelsize(z)]-1)/image.zoom_colpixelsize(z));
              for (uint32_t c = begin; c < end; c++) {
                    if (adaptive && ((map(0,z,r-1,c) == 255) || (map(0,z,r+1,c) == 255))) continue;
                    if (alphazero && p<3 && image(3,z,r,c) == 0) continue;
                    if (FRA && p<4 && image(4,z,r,c) > 0) continue;
                    ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max,predictor);
                    ColorVal curr = image(p,z,r,c);
                    int actual_loss = (adaptive? factor-map(0,z,r,c) : factor) * lossp[p] / luma_alpha_compensate(p,z,loss,image(0,z,r,c),image(p,z,r,c),(nump>3?image(3,z,r,c):255));
                    // example: assume actual_loss > 30, then:
                    //  top:    100                                                     90    (error:0 -> 10)
                    //  curr:   120  -->  150 (guess)    so we compensate and make it   140   (error:30 -> 20)
                    //  bottom: 200                                                     190   (error:0 -> 10)
                    // other example:
                    //  top:    100                                                     110   (error: 0 -> 10)
                    //  curr:   120  -->  90 (guess)     so we compensate and make it   100   (error: 30 -> 20)
                    //  bottom: 80                                                      90    (error: 0 -> 10)
                    ColorVal diff = curr-guess;
                    if (abs(diff) > actual_loss*6/5) continue;
//                    if (abs(diff) > actual_loss) continue;
                    if (z>0 && abs(diff) > actual_loss) continue;
                    ColorVal top = image(p,z,r-1,c);
                    ColorVal bottom = image(p,z,r+1,c);
                    top += diff/3;
                    bottom += diff/3;
                    for (int pp=0; pp<p; pp++) properties[pp] = image(pp,z,r-1,c);
                    ranges->snap(p,properties,min,max,top);
                    for (int pp=0; pp<p; pp++) properties[pp] = image(pp,z,r+1,c);
                    ranges->snap(p,properties,min,max,bottom);
                    image.set(p,z,r-1,c, top);
                    image.set(p,z,r+1,c, bottom);
              }
            }
          }
      } else {
          for (uint32_t r = 0; r < images[0].rows(z); r++) {
            for (int fr=0; fr<(int)images.size(); fr++) {
              Image& image = images[fr];
              if (image.seen_before >= 0) { continue; }
              uint32_t begin=(image.col_begin[r*image.zoom_rowpixelsize(z)]/image.zoom_colpixelsize(z)),
                         end=(1+(image.col_end[r*image.zoom_rowpixelsize(z)]-1)/image.zoom_colpixelsize(z))|1;
              if (begin>1 && ((begin&1) ==0)) begin--;
              if (begin==0) begin=1;
              for (uint32_t c = begin; c < end-1; c+=2) {
                    if (adaptive && ((map(0,z,r,c-1) == 255) || (map(0,z,r,c+1)==255))) continue;
                    if (alphazero && p<3 && image(3,z,r,c) == 0) continue;
                    if (FRA && p<4 && image(4,z,r,c) > 0) continue;
                    ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max,predictor);
                    ColorVal curr = image(p,z,r,c);
                    int actual_loss = (adaptive? factor-map(0,z,r,c) : factor) * lossp[p] / luma_alpha_compensate(p,z,loss,image(0,z,r,c),image(p,z,r,c),(nump>3?image(3,z,r,c):255));
                    ColorVal diff = curr-guess;
                    if (abs(diff) > actual_loss) continue;
                    ColorVal left = image(p,z,r,c-1);
                    ColorVal right = image(p,z,r,c+1);
                    left += diff/3;
                    right += diff/3;
                    for (int pp=0; pp<p; pp++) properties[pp] = image(pp,z,r,c-1);
                    ranges->snap(p,properties,min,max,left);
                    for (int pp=0; pp<p; pp++) properties[pp] = image(pp,z,r,c+1);
                    ranges->snap(p,properties,min,max,right);
                    image.set(p,z,r,c-1, left);
                    image.set(p,z,r,c+1, right);
              }
            }
          }
      }
    }

    // add loss
    for (int i = 0; i < plane_zoomlevels(images[0], beginZL, endZL); i++) {
      std::pair<int, int> pzl = plane_zoomlevel(images[0], beginZL, endZL, i, ranges);
      int p = pzl.first;
      int z = pzl.second;
      if (ranges->min(p) >= ranges->max(p)) continue;
      Properties properties((nump>3?NB_PROPERTIESA[p]:NB_PROPERTIES[p]));
//      int lossp[] = {(loss+6)/10, (loss+2)/4, (loss+2)/3, loss/10, 0};  // less loss on Y, more on Co and Cg
      if (lossp[p]==0) continue;
      if (z % 2 == 0) {
          for (uint32_t r = 1; r < images[0].rows(z); r += 2) {
            for (int fr=0; fr<(int)images.size(); fr++) {
              Image& image = images[fr];
              if (image.seen_before >= 0) { continue; }
              uint32_t begin=(image.col_begin[r*image.zoom_rowpixelsize(z)]/image.zoom_colpixelsize(z)),
                         end=(1+(image.col_end[r*image.zoom_rowpixelsize(z)]-1)/image.zoom_colpixelsize(z));
              for (uint32_t c = begin; c < end; c++) {
                    if (adaptive && (map(0,z,r,c) == 255)) continue;
                    if (alphazero && p<3 && image(3,z,r,c) == 0) continue;
                    if (FRA && p<4 && image(4,z,r,c) > 0) continue;
                    ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max,predictor);
                    ColorVal curr = image(p,z,r,c);
                    int factor=255;
                    if (z>2 && c+1 < end && c > begin) {
                        ColorVal child1 = image(p,z-1,r,2*c-1);
                        ColorVal child2 = image(p,z-1,r,2*c+1);
//                        if (abs(curr-child1) <= loss && abs(curr-child2) <= loss) {
                        if (abs(curr-child1) < 2*lossp[p] && abs(curr-child2) < 2*lossp[p] ) {
                            // current pixel is likely to influence neighbors in next zoomlevels, so apply less loss to avoid cascading error
                            factor = 64+(abs(curr-child1)+abs(curr-child2)+abs(curr-guess))*64/lossp[p];
                            if (factor>255) factor=255;
                        }
                    }
                    // less error in the early steps
                    if (z>4) factor = factor * 4 / z;
//                    if (z>6 && lossp[p]>30) factor /= 2;
//                    if (z>8 && lossp[p]>20) factor /= 2;
                    factor = ((beginZL-z)*factor/(beginZL));
                    if (z==0) factor += loss*2;
                    ColorVal diff = flif_make_lossy(min - guess, max - guess, curr - guess,
                                        (adaptive? factor-map(0,z,r,c) : factor) * lossp[p] / luma_alpha_compensate(p,z,loss,image(0,z,r,c),image(p,z,r,c),(nump>3?image(3,z,r,c):255)));
                    ColorVal lossyval = guess+diff;
                    ranges->snap(p,properties,min,max,lossyval);
                    image.set(p,z,r,c, lossyval);
              }
            }
          }
      } else {
        // vertical: scan the odd columns
          for (uint32_t r = 0; r < images[0].rows(z); r++) {
            for (int fr=0; fr<(int)images.size(); fr++) {
              Image& image = images[fr];
              if (image.seen_before >= 0) { continue; }
              uint32_t begin=(image.col_begin[r*image.zoom_rowpixelsize(z)]/image.zoom_colpixelsize(z)),
                         end=(1+(image.col_end[r*image.zoom_rowpixelsize(z)]-1)/image.zoom_colpixelsize(z))|1;
              if (begin>1 && ((begin&1) ==0)) begin--;
              if (begin==0) begin=1;
              for (uint32_t c = begin; c < end; c+=2) {
                    if (adaptive && (map(0,z,r,c) == 255)) continue;
                    if (alphazero && p<3 && image(3,z,r,c) == 0) continue;
                    if (FRA && p<4 && image(4,z,r,c) > 0) continue;
                    ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max,predictor);
                    ColorVal curr = image(p,z,r,c);
                    int factor=255;
                    if (z>2 && r+1 < images[0].rows(z) && r > 0) {
                        ColorVal child1 = image(p,z-1,2*r-1,c);
                        ColorVal child2 = image(p,z-1,2*r+1,c);
//                        if (abs(curr-child1) <= loss && abs(curr-child2) <= loss) {
                        if (abs(curr-child1) < 2*lossp[p] && abs(curr-child2) < 2*lossp[p] ) {
                            // current pixel is likely to influence neighbors in next zoomlevels, so apply less loss to avoid cascading error
                            factor = 64+(abs(curr-child1)+abs(curr-child2)+abs(curr-guess))*64/lossp[p];
                            if (factor>255) factor=255;
                        }
                    }
                    if (z>4) factor = factor * 4 / z;
//                    if (z>6 && lossp[p]>30) factor /= 2;
//                    if (z>8 && lossp[p]>20) factor /= 2;
                    factor = ((beginZL-z)*factor/(beginZL));
                    if (z==1) factor += loss;
                    ColorVal diff = flif_make_lossy(min - guess, max - guess, curr - guess,
                                        (adaptive? factor-map(0,z,r,c) : factor) * lossp[p] / luma_alpha_compensate(p,z,loss,image(0,z,r,c),image(p,z,r,c),(nump>3?image(3,z,r,c):255)));
                    ColorVal lossyval = guess+diff;
                    ranges->snap(p,properties,min,max,lossyval);
                    image.set(p,z,r,c, lossyval);
              }
            }
          }
      }
    }
}

template<typename IO, typename BitChance, typename Rac> void flif_encode_tree(IO& io, Rac &rac, const ColorRanges *ranges, const std::vector<Tree> &forest, const flifEncoding encoding)
{
    for (int p = 0; p < ranges->numPlanes(); p++) {
        Ranges propRanges;
        if (encoding==flifEncoding::nonInterlaced) initPropRanges_scanlines(propRanges, *ranges, p);
        else initPropRanges(propRanges, *ranges, p);
        MetaPropertySymbolCoder<BitChance, Rac> metacoder(rac, propRanges);
//        forest[p].print(stdout);
        if (ranges->min(p)<ranges->max(p))
        metacoder.write_tree(forest[p]);
    }
}
template <int bits, typename IO>
void flif_encode_main(RacOut<IO>& rac, IO& io, Images &images, const ColorRanges *ranges, flif_options &options) {

    flifEncoding encoding = options.method.encoding;
    int learn_repeats = options.learn_repeats;
    Image& image=images[0];
    int realnumplanes = 0;
    for (int i=0; i<ranges->numPlanes(); i++) if (ranges->min(i)<ranges->max(i)) realnumplanes++;
    pixels_todo = (int64_t)image.rows()*image.cols()*realnumplanes*(learn_repeats+1);
    for (int i=1; i<ranges->numPlanes(); i++)
        if (options.chroma_subsampling && ranges->min(i)<ranges->max(i))
            pixels_todo -= (image.rows()*image.cols()-image.rows(2)*image.cols(2))*(learn_repeats+1);
    pixels_done = 0;
    if (pixels_todo == 0) pixels_todo = pixels_done = 1;

    // two passes
    std::vector<Tree> forest(ranges->numPlanes(), Tree());
    RacDummy dummy;


    long fs = io.ftell();

    int roughZL = 0;
    if (encoding == flifEncoding::interlaced) {
      roughZL = image.zooms() - NB_NOLEARN_ZOOMS-1;
      if (roughZL < 0) roughZL = 0;
      //v_printf(2,"Encoding rough data\n");
      UniformSymbolCoder<RacOut<IO>> metaCoder(rac);
      metaCoder.write_int(0,image.zooms(),roughZL);
      flif_encode_FLIF2_pass<IO, RacOut<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut<IO>, bits> >(io, rac, images, ranges, forest, image.zooms(), roughZL+1, 1, options);
    }

    //v_printf(2,"Encoding data (pass 1)\n");
    if (learn_repeats>0) v_printf(3,"Learning a MANIAC tree. Iterating %i time%s.\n",learn_repeats,(learn_repeats>1?"s":""));
    switch(encoding) {
        case flifEncoding::nonInterlaced:
           flif_encode_scanlines_pass<IO, RacDummy, PropertySymbolCoder<FLIFBitChancePass1, RacDummy, bits> >(io, dummy, images, ranges, forest, learn_repeats, options);
           break;
        case flifEncoding::interlaced:
           flif_encode_FLIF2_pass<IO, RacDummy, PropertySymbolCoder<FLIFBitChancePass1, RacDummy, bits> >(io, dummy, images, ranges, forest, roughZL, 0, learn_repeats, options);
           break;
    }
    v_printf_tty(3,"\r");
    v_printf(3,"Header: %li bytes.", fs);
    if (encoding==flifEncoding::interlaced) v_printf(3," Rough data: %li bytes.", io.ftell()-fs);
    fflush(stdout);

    //v_printf(2,"Encoding tree\n");
    fs = io.ftell();
    flif_encode_tree<IO, FLIFBitChanceTree, RacOut<IO>>(io, rac, ranges, forest, encoding);
    v_printf(3," MANIAC tree: %li bytes.\n", io.ftell()-fs);
    options.divisor=0;
    options.min_size=0;
    options.split_threshold=0;
    //v_printf(2,"Encoding data (pass 2)\n");
    switch(encoding) {
        case flifEncoding::nonInterlaced:
           flif_encode_scanlines_pass<IO, RacOut<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut<IO>, bits> >(io, rac, images, ranges, forest, 1, options);
           break;
        case flifEncoding::interlaced:
           flif_encode_FLIF2_pass<IO, RacOut<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut<IO>, bits> >(io, rac, images, ranges, forest, roughZL, 0, 1, options);
           break;
    }

}

template <typename IO>
void write_big_endian_varint(IO& io, unsigned long number, bool done = true) {
    if (number < 128) {
        if (done) io.fputc(number);
        else io.fputc(number + 128);
    } else {
        unsigned long lsb = (number & 127);
        number >>= 7;
        write_big_endian_varint(io, number, false);
        write_big_endian_varint(io, lsb, done);
    }
}

template <typename IO>
void write_chunk(IO& io, MetaData& metadata) {
//    printf("chunk: %s\n", metadata.name);
//    printf("chunk length: %lu\n", metadata.length);
    io.fputs(metadata.name);
    unsigned long length = metadata.length;
    write_big_endian_varint(io, length);
    for(unsigned long i = 0; i < length; i++) {
        io.fputc(metadata.contents[i]);
    }
}


template <typename IO>
bool flif_encode(IO& io, Images &images, const std::vector<std::string> &transDesc, flif_options &options) {

    flifEncoding encoding = options.method.encoding;

    int numPlanes = images[0].numPlanes();
    int numFrames = images.size();
#ifndef SUPPORT_ANIMATION
    if (numFrames > 1) {
        e_printf("This FLIF cannot encode animations. Please compile with SUPPORT_ANIMATION.\n");
        return false;
    }
#endif

    bool adaptive = (options.loss<0);
    Image adaptive_map;
    if (adaptive) { // images[0] is the still image to be encoded, images[1] is the saliency map for adaptive lossy
        options.loss = -options.loss;
        if (numFrames != 2) { e_printf("Expected two input images: [IMAGE] [ADAPTIVE-MAP]\n"); return false; }
        adaptive_map = images[1].clone();
        numFrames = 1;
        images.pop_back();
    }


    io.fputs("FLIF");  // bytes 1-4 are fixed magic
    // byte 5 encodes color type, interlacing, animation
    // 128 64 32 16 8 4 2 1
    //                                                Gray8    RGB24    RGBA32    Gray16   RGB48    RGBA64
    //      0  1  1           = scanlines, still     (FLIF11,  FLIF31,  FLIF41,   FLIF12,  FLIF32,  FLIF42)
    //      1  0  0           = interlaced, still    (FLIFA1,  FLIFC1,  FLIFD1,   FLIFA2,  FLIFC2,  FLIFD2)
    //      1  0  1           = scanlines, animated  (FLIFQ1,  FLIFS1,  FLIFT1,   FLIFQ2,  FLIFS2,  FLIFT2)
    //      1  1  0           = interlaced, animated (FLIFa1,  FLIFc1,  FLIFd1,   FLIFa2,  FLIFc2,  FLIFd2)
    //      0  0  0           = interlaced, still, non-default interlace order (not yet implemented)
    //      1  1  1           = interlaced, animated, non-default interlace order (not yet implemented)
    //              0         = RGB(A) color model (can be transformed to other color spaces like YCoCg/YCbCr)
    //              1         = other color models, e.g. CMYK, RGBE (not yet implemented)
    //              0 0 0 1   = grayscale (1 plane)
    //              0 0 1 1   = RGB (3 planes)
    //              0 1 0 0   = RGBA (4 planes)       (grayscale + alpha is encoded as RGBA to keep the number of cases low)
    //   0                    = MANIAC trees with default context properties
    //   1                    = (not yet implemented; could be used for other entropy coding methods)
    int c=' '+16*(static_cast<uint8_t>(encoding))+numPlanes;
    if (numFrames>1) c += 32;
    io.fputc(c);

    // next byte (byte 6) encodes the bit depth:
    // '1' for 8 bits (1 byte) per channel, '2' for 16 bits (2 bytes) per channel, '0' for custom bit depth

    c='1';
    for (int p = 0; p < numPlanes; p++) {if (images[0].max(p) != 255) c='2';}
    if (c=='2') {for (int p = 0; p < numPlanes; p++) {if (images[0].max(p) != 65535) c='0';}}
    io.fputc(c);

    Image& image = images[0];

    // encode width and height in a variable number of bytes
    write_big_endian_varint(io, image.cols() - 1);
    write_big_endian_varint(io, image.rows() - 1);

    // for animations: number of frames
    if (numFrames>1) {
        write_big_endian_varint(io, numFrames - 2);
    }

    for(size_t i=0; i<images[0].metadata.size(); i++) {
            write_chunk(io,images[0].metadata[i]);
            v_printf(3,"Encoded metadata chunk: %s\n",images[0].metadata[i].name);
    }

    // marker to indicate FLIF version (version 0 aka FLIF16 in this case)
    io.fputc(0);


    RacOut<IO> rac(io);
    //SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18> metaCoder(rac);
    UniformSymbolCoder<RacOut<IO>> metaCoder(rac);

//    metaCoder.write_int(0,1,0); // FLIF version: FLIF16

    v_printf(2," (%ux%u", images[0].cols(), images[0].rows());
    for (int p = 0; p < numPlanes; p++) {
        assert(image.min(p) == 0);
        if (c=='0') {
          metaCoder.write_int(1, 16, ilog2(image.max(p)+1));
          v_printf(3," [%i] %i bpp",p,ilog2(image.max(p)+1));
        }
    }
    if (c=='1') { v_printf(3,", %i channels, 8-bit",numPlanes); }
    else if (c=='2') { v_printf(3,", %i channels, 16-bit",numPlanes); }
    if (numFrames>1) v_printf(3,", %i frames",numFrames);
    int alphazero=0;
    if (numPlanes > 3) {
        if (images[0].alpha_zero_special) alphazero=1;
        if (alphazero) metaCoder.write_int(0,1,1);
        else metaCoder.write_int(0,1,0);
        if (!alphazero) v_printf(3, ", keep RGB at A=0");
    }
    v_printf(2,")\n");
    if (numFrames>1) {
        metaCoder.write_int(0, 100, 0); // repeats (0=infinite)
        for (int i=0; i<numFrames; i++) {
           metaCoder.write_int(0, 60000, images[i].frame_delay); // time in ms between frames
        }
    }

    if (options.cutoff==2 && options.alpha==19) {
      metaCoder.write_int(0,1,0); // using default constants for cutoff/alpha
    } else {
      metaCoder.write_int(0,1,1); // not using default constants
      metaCoder.write_int(1,128,options.cutoff);
      metaCoder.write_int(2,128,options.alpha);
      metaCoder.write_int(0,1,0); // using default initial bitchances (non-default values has not been implemented yet!)
    }
    options.alpha = 0xFFFFFFFF/options.alpha;

    uint32_t checksum = 0;
    if (images[0].palette) options.crc_check = false;
    if (options.crc_check && !options.loss) {
      if (alphazero) for (Image& i : images) i.make_invisible_rgb_black();
      checksum = image.checksum(); // if there are multiple frames, the checksum is based only on the first frame.
    }

    std::vector<std::unique_ptr<const ColorRanges>> rangesList;
    rangesList.push_back(std::unique_ptr<const ColorRanges>(getRanges(image)));
    int tcount=0;
    v_printf(3,"Transforms: ");

    std::vector<std::unique_ptr<Transform<IO>>> transforms;

    int warn_about_incompatibility = 0;
    try {
      for (unsigned int i=0; i<transDesc.size(); i++) {
        auto trans = create_transform<IO>(transDesc[i]);
        auto previous_range = rangesList.back().get();
        if (transDesc[i] == "Palette" || transDesc[i] == "Palette_Alpha") trans->configure(options.palette_size);
#ifdef SUPPORT_ANIMATION
        if (transDesc[i] == "Frame_Lookback") trans->configure(options.lookback);
#endif
        if (transDesc[i] == "PermutePlanes") trans->configure(options.subtract_green);
        if (!trans->init(previous_range) ||
            (!trans->process(previous_range, images)
              && !(options.acb==1 && transDesc[i] == "Color_Buckets" && (v_printf(3,", forced "), (tcount=0), true) ))) {
            //e_printf( "Transform '%s' failed\n", transDesc[i].c_str());
            if (images[0].palette && transDesc[i] == "Palette_Alpha" && options.keep_palette) {
                v_printf(2,"Could not keep palette for some reason. Aborting.\n");
                return false;
            }
        } else {
            if (tcount++ > 0) v_printf(3,", ");
            v_printf(3,"%s", transDesc[i].c_str());
            fflush(stdout);
            rac.write_bit(true);
            write_name(rac, transDesc[i]);
            trans->save(previous_range, rac);
            fflush(stdout);
            rangesList.push_back(std::unique_ptr<const ColorRanges>(trans->meta(images, previous_range)));
            trans->data(images);
            if (transDesc[i] == "Color_Buckets") warn_about_incompatibility = 1;
            if (warn_about_incompatibility && (transDesc[i] == "Frame_Lookback" || transDesc[i] == "Duplicate_Frame" || transDesc[i] == "Frame_Shape"))
                warn_about_incompatibility = 2;
            if (options.just_add_loss) transforms.push_back(std::move(trans));
        }
      }
    } catch (std::bad_alloc& ba) {
         e_printf("Error: could not allocate enough memory for transforms. Aborting. Try -E0.\n");
         return false;
    }
    if (tcount==0) v_printf(3,"none\n"); else v_printf(3,"\n");
    if (warn_about_incompatibility > 1) v_printf(1,"WARNING: This animated FLIF will probably not be properly decoded by older FLIF decoders (version < 0.3) since they have a bug in this particular combination of transformations.\nIf backwards compatibility is important, you can use the option -B to avoid the issue.\n");
    rac.write_bit(false);
    const ColorRanges* ranges = rangesList.back().get();

    for (int p = 0; p < ranges->numPlanes(); p++) {
      v_printf(7,"Plane %i: %i..%i\n",p,ranges->min(p),ranges->max(p));
    }

    int mbits = 0;
    for (int p = 0; p < ranges->numPlanes(); p++) {
        if (ranges->max(p) > ranges->min(p)) {
          int nBits = ilog2((ranges->max(p) - ranges->min(p))*2-1)+1;
          if (nBits > mbits) mbits = nBits;
        }
    }
    int bits = 10; // hardcoding things for 8 bit RGB (which means 9 bit Co/Cg and 10 bit differences)
#ifdef SUPPORT_HDR
    if (mbits >10) bits=18;
#endif
    if (mbits > bits) { e_printf("OOPS: this FLIF only supports 8-bit RGBA (not compiled with SUPPORT_HDR)\n"); return false;}

    if (alphazero && ranges->numPlanes() > 3 && ranges->min(3) <= 0) {
      v_printf(4,"Replacing fully transparent subpixels with predicted subpixel values\n");
      switch(encoding) {
        case flifEncoding::nonInterlaced: flif_encode_scanlines_interpol_zero_alpha(images, ranges); break;
        case flifEncoding::interlaced:
            v_printf(4,"Invisible pixel predictor: -H%i\n",options.invisible_predictor);
            metaCoder.write_int(0,MAX_PREDICTOR,options.invisible_predictor);
            flif_encode_FLIF2_interpol_zero_alpha(images, ranges, image.zooms(), 0, options.invisible_predictor);
            break;
      }
    }

    for (int p = 1; p < ranges->numPlanes(); p++) {
        if (ranges->min(p) >= ranges->max(p)) {
            //v_printf(5,"Constant plane %i at color value %i\n",p,ranges->min(p));
            //pixels_todo -= (int64_t)image.rows()*image.cols()*(learn_repeats+1);
            for (int fr = 0; fr < numFrames; fr++)
                images[fr].make_constant_plane(p,ranges->min(p));
        }
    }
    // Y plane shouldn't be constant, even if it is (because we want to avoid special-casing fast Y plane access)
    bool smaller_buffer = false;
    if (images[0].palette && ranges->max(1) < 256 && options.keep_palette && (ranges->numPlanes() < 4 || ranges->min(3)==ranges->max(3))) smaller_buffer = true;
    if (!smaller_buffer) for (int fr = 0; fr < numFrames; fr++) images[fr].undo_make_constant_plane(0);

    if (options.loss>0) for(int p=0; p<numPlanes; p++) options.predictor[p]=0;

    if (options.loss > 0) {
      v_printf(3,"Introducing loss to improve compression. Amount of loss: %i\n", options.loss);
      switch(encoding) {
        case flifEncoding::nonInterlaced:
            // this is probably a bad idea, the artifacts are ugly
            flif_make_lossy_scanlines(images,ranges,options.loss,adaptive,adaptive_map);
            if (alphazero && ranges->numPlanes() > 3 && ranges->min(3) <= 0) flif_encode_scanlines_interpol_zero_alpha(images, ranges);
            break;
        case flifEncoding::interlaced:
            flif_make_lossy_interlaced(images,ranges,options.loss,adaptive,adaptive_map);
            if (alphazero && ranges->numPlanes() > 3 && ranges->min(3) <= 0) flif_encode_FLIF2_interpol_zero_alpha(images, ranges, image.zooms(), 0, options.invisible_predictor);
            break;
      }
      if (options.just_add_loss) {
        while(!transforms.empty()) {
          transforms.back()->invData(images);
          transforms.pop_back();
        }
        return true;
      }
    } else {
        if (encoding == flifEncoding::interlaced) {
          v_printf(3,"Using pixel predictor method: -G");
          bool autodetect=false;
          for(int p=0; p<ranges->numPlanes(); p++) {
            if (options.predictor[p] >= 0) v_printf(3,"%i",options.predictor[p]);
            else if (options.predictor[p] == -1) v_printf(3,"X");
            else {v_printf(3,"?"); autodetect=true;}
          }
          if (autodetect) {
           v_printf(3,"  ->  -G");
           for(int p=0; p<ranges->numPlanes(); p++) {
            if (options.predictor[p] == -2) {
              if (ranges->min(p) < ranges->max(p)) {
                options.predictor[p] = find_best_predictor(images, ranges, p, 1);
                // predictor 0 is usually the safest choice, so only pick a different one if it's the best at zoomlevel 0 too
                if (options.predictor[p] > 0 && find_best_predictor(images, ranges, p, 0) != options.predictor[p]) options.predictor[p] = 0;
              } else options.predictor[p] = 0;
            }
            if (options.predictor[p] >= 0) v_printf(3,"%i",options.predictor[p]);
            else if (options.predictor[p] == -1) v_printf(3,"X");
           }
          }
          v_printf(3,"\n");
        }
    }


    if (bits ==10) {
      flif_encode_main<10>(rac, io, images, ranges, options);
#ifdef SUPPORT_HDR
    } else {
      flif_encode_main<18>(rac, io, images, ranges, options);
#endif
    }

    if (options.crc_check && !options.loss && (options.crc_check>0 || io.ftell() > 100) && !options.chroma_subsampling) {
      v_printf(2,"Writing checksum: %X\n", checksum);
      metaCoder.write_int(0,1,1);
      metaCoder.write_int(16, (checksum >> 16) & 0xFFFF);
      metaCoder.write_int(16, checksum & 0xFFFF);
    } else {
      v_printf(2,"Not writing checksum\n");
      metaCoder.write_int(0,1,0); // don't write checksum for tiny images or when asked not to
    }
    rac.flush();
    io.flush();

    v_printf_tty(2,"\r");
    if (numFrames==1)
      v_printf(2,"Wrote output FLIF file %s, %li bytes for %ux%u pixels (%.4fbpp)   \n",io.getName(),io.ftell(), images[0].cols(), images[0].rows(), 8.0*io.ftell()/images[0].rows()/images[0].cols());
    else
      v_printf(2,"Wrote output FLIF file %s, %li bytes for %i frames of %ux%u pixels (%.4fbpp)   \n",io.getName(),io.ftell(), numFrames, images[0].cols(), images[0].rows(), 8.0*io.ftell()/numFrames/images[0].rows()/images[0].cols());

//    images[0].save("debug.pam");

    return true;
}


template bool flif_encode(FileIO& io, Images &images, const std::vector<std::string> &transDesc, flif_options &options);
template bool flif_encode(BlobIO& io, Images &images, const std::vector<std::string> &transDesc, flif_options &options);

#endif
