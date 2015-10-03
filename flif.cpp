/*  FLIF - Free Lossless Image Format
 Copyright (C) 2010-2015  Jon Sneyers & Pieter Wuille

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 Parts of this code are based on code from the FFMPEG project, in
 particular parts:
 - ffv1.c - Copyright (c) 2003 Michael Niedermayer <michaelni@gmx.at>
 - common.h - copyright (c) 2006 Michael Niedermayer <michaelni@gmx.at>
 - rangecoder.c - Copyright (c) 2004 Michael Niedermayer <michaelni@gmx.at>
*/

#include <string>
#include <string.h>

#include "maniac/rac.h"
#include "maniac/compound.h"
#include "maniac/util.h"

#include "image/color_range.h"
#include "transform/factory.h"

#include "flif_config.h"

#include "getopt.h"
#include <stdarg.h>


static FILE *f;  // the compressed file

static std::vector<ColorVal> grey; // a pixel with values in the middle of the bounds
static int64_t pixels_todo = 0;
static int64_t pixels_done = 0;
static int verbosity = 1;

typedef SimpleBitChance                         FLIFBitChancePass1;

// faster:
//typedef SimpleBitChance                         FLIFBitChancePass2;
//typedef SimpleBitChance                         FLIFBitChanceParities;

// better compression:
typedef MultiscaleBitChance<6,SimpleBitChance>  FLIFBitChancePass2;
typedef MultiscaleBitChance<6,SimpleBitChance>  FLIFBitChanceParities;

typedef MultiscaleBitChance<6,SimpleBitChance>  FLIFBitChanceTree;


#define MAX_TRANSFORM 8
const std::vector<std::string> transforms = {"YIQ","BND","ACB","PLT","PLA","FRS","DUP","FRA","???"};
template<typename RAC> void static write_name(RAC& rac, std::string desc)
{
    int nb=0;
    while (nb <= MAX_TRANSFORM) {
        if (transforms[nb] == desc) break;
        nb++;
    }
    if (transforms[nb] != desc) { fprintf(stderr,"ERROR: Unknown transform description string!\n"); return;}
    UniformSymbolCoder<RAC> coder(rac);
    coder.write_int(0, MAX_TRANSFORM, nb);
}

template<typename RAC> std::string static read_name(RAC& rac)
{
    UniformSymbolCoder<RAC> coder(rac);
    int nb = coder.read_int(0, MAX_TRANSFORM);
    return transforms[nb];
}


void v_printf(const int v, const char *format, ...) {
    if (verbosity < v) return;
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    fflush(stdout);
    va_end(args);
}

// planes:
// 0    Y channel (luminance)
// 1    I (chroma)
// 2    Q (chroma)
// 3    Alpha (transparency)


/******************************************/
/*   scanlines encoding/decoding          */
/******************************************/


const int NB_PROPERTIES_scanlines[] = {7,8,9,7};
const int NB_PROPERTIES_scanlinesA[] = {8,9,10,7};
void static initPropRanges_scanlines(Ranges &propRanges, const ColorRanges &ranges, int p)
{
    propRanges.clear();
    int min = ranges.min(p);
    int max = ranges.max(p);
    int mind = min - max, maxd = max - min;

    if (p != 3) {
      for (int pp = 0; pp < p; pp++) {
        propRanges.push_back(std::make_pair(ranges.min(pp), ranges.max(pp)));  // pixels on previous planes
      }
      if (ranges.numPlanes()>3) propRanges.push_back(std::make_pair(ranges.min(3), ranges.max(3)));  // pixel on alpha plane
    }
    propRanges.push_back(std::make_pair(min,max));   // guess (median of 3)
    propRanges.push_back(std::make_pair(0,3));       // which predictor was it
    propRanges.push_back(std::make_pair(mind,maxd));
    propRanges.push_back(std::make_pair(mind,maxd));
    propRanges.push_back(std::make_pair(mind,maxd));
    propRanges.push_back(std::make_pair(mind,maxd));
    propRanges.push_back(std::make_pair(mind,maxd));
}



ColorVal predict_and_calcProps_scanlines(Properties &properties, const ColorRanges *ranges, const Image &image, const int p, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max) {
    ColorVal guess;
    int which = 0;
    int index=0;
    if (p != 3) {
      for (int pp = 0; pp < p; pp++) {
        properties[index++] = image(pp,r,c);
      }
      if (image.numPlanes()>3) properties[index++] = image(3,r,c);
    }
    ColorVal left = (c>0 ? image(p,r,c-1) : grey[p]);;
    ColorVal top = (r>0 ? image(p,r-1,c) : grey[p]);
    ColorVal topleft = (r>0 && c>0 ? image(p,r-1,c-1) : grey[p]);
    ColorVal gradientTL = left + top - topleft;
    guess = median3(gradientTL, left, top);
    ranges->snap(p,properties,min,max,guess);
    if (guess == gradientTL) which = 0;
    else if (guess == left) which = 1;
    else if (guess == top) which = 2;

    properties[index++] = guess;
    properties[index++] = which;

    if (c > 0 && r > 0) { properties[index++] = left - topleft; properties[index++] = topleft - top; }
                 else   { properties[index++] = 0; properties[index++] = 0;  }

    if (c+1 < image.cols() && r > 0) properties[index++] = top - image(p,r-1,c+1); // top - topright
                 else   properties[index++] = 0;
    if (r > 1) properties[index++] = image(p,r-2,c)-top;    // toptop - top
         else properties[index++] = 0;
    if (c > 1) properties[index++] = image(p,r,c-2)-left;    // leftleft - left
         else properties[index++] = 0;
    return guess;
}

template<typename Coder> void encode_scanlines_inner(std::vector<Coder*> &coders, const Images &images, const ColorRanges *ranges)
{
    ColorVal min,max;
    long fs = ftell(f);
    long pixels = images[0].cols()*images[0].rows()*images.size();
    int nump = images[0].numPlanes();
    int beginp = (nump>3 ? 3 : 0);
    for (int p = beginp, i=0; i++ < nump; p = (p+1)%nump) {
        Properties properties((nump>3?NB_PROPERTIES_scanlinesA[p]:NB_PROPERTIES_scanlines[p]));
        v_printf(2,"\r%i%% done [%i/%i] ENC[%ux%u]    ",(int)(100*pixels_done/pixels_todo),i,nump,images[0].cols(),images[0].rows());
        pixels_done += images[0].cols()*images[0].rows();
        if (ranges->min(p) >= ranges->max(p)) continue;
        for (uint32_t r = 0; r < images[0].rows(); r++) {
            for (int fr=0; fr< (int)images.size(); fr++) {
              const Image& image = images[fr];
              if (image.seen_before >= 0) continue;
              uint32_t begin=image.col_begin[r], end=image.col_end[r];
              for (uint32_t c = begin; c < end; c++) {
                if (nump>3 && p<3 && image(3,r,c) <= 0) continue;
                ColorVal guess = predict_and_calcProps_scanlines(properties,ranges,image,p,r,c,min,max);
                ColorVal curr = image(p,r,c);
                assert(p != 3 || curr >= -fr);
                if (p==3 && min < -fr) min = -fr;
                coders[p]->write_int(properties, min - guess, max - guess, curr - guess);
              }
            }
        }
        long nfs = ftell(f);
        if (nfs-fs > 0) {
           v_printf(3,"filesize : %li (+%li for %li pixels, %f bpp)", nfs, nfs-fs, pixels, 8.0*(nfs-fs)/pixels );
           v_printf(4,"\n");
        }
        fs = nfs;
    }
}

template<typename Rac, typename Coder> void encode_scanlines_pass(Rac &rac, const Images &images, const ColorRanges *ranges, std::vector<Tree> &forest, int repeats)
{
    std::vector<Coder*> coders;

    for (int p = 0; p < ranges->numPlanes(); p++) {
        Ranges propRanges;
        initPropRanges_scanlines(propRanges, *ranges, p);
        coders.push_back(new Coder(rac, propRanges, forest[p]));
    }

    while(repeats-- > 0) {
     encode_scanlines_inner(coders, images, ranges);
    }

    for (int p = 0; p < ranges->numPlanes(); p++) {
        coders[p]->simplify();
    }

    for (int p = 0; p < ranges->numPlanes(); p++) {
#ifdef STATS
        indent(0); v_printf(2,"Plane %i\n", p);
        coders[p]->info(0+1);
#endif
        delete coders[p];
    }
}


void encode_scanlines_interpol_zero_alpha(Images &images, const ColorRanges *ranges)
{

    ColorVal min,max;
    int nump = images[0].numPlanes();
    if (nump > 3)
    for (Image& image : images)
    for (int p = 0; p < 3; p++) {
        Properties properties((nump>3?NB_PROPERTIES_scanlinesA[p]:NB_PROPERTIES_scanlines[p]));
        if (ranges->min(p) >= ranges->max(p)) continue;
//          v_printf(2,"[%i] interpol_zero_alpha ",p);
//        fflush(stdout);
        for (uint32_t r = 0; r < image.rows(); r++) {
            for (uint32_t c = 0; c < image.cols(); c++) {
                if (image(3,r,c) == 0) {
                    image.set(p,r,c, predict_and_calcProps_scanlines(properties,ranges,image,p,r,c,min,max));
                }
            }
        }
    }
//    v_printf(2,"\n");
}


template<typename Coder> void decode_scanlines_inner(std::vector<Coder*> &coders, Images &images, const ColorRanges *ranges)
{

    ColorVal min,max;
    int nump = images[0].numPlanes();
    int beginp = (nump>3 ? 3 : 0);
    for (int p = beginp, i=0; i++ < nump; p = (p+1)%nump) {
        Properties properties((nump>3?NB_PROPERTIES_scanlinesA[p]:NB_PROPERTIES_scanlines[p]));
        v_printf(2,"\r%i%% done [%i/%i] DEC[%ux%u]    ",(int)(100*pixels_done/pixels_todo),i,nump,images[0].cols(),images[0].rows());
        v_printf(4,"\n");
        pixels_done += images[0].cols()*images[0].rows();
        if (ranges->min(p) >= ranges->max(p)) continue;
        for (uint32_t r = 0; r < images[0].rows(); r++) {
            for (int fr=0; fr< (int)images.size(); fr++) {
              Image& image = images[fr];
              uint32_t begin=image.col_begin[r], end=image.col_end[r];
              if (image.seen_before >= 0) { for(uint32_t c=0; c<image.cols(); c++) image.set(p,r,c,images[image.seen_before](p,r,c)); continue; }
              if (fr>0) {
                for (uint32_t c = 0; c < begin; c++)
                   if (nump>3 && p<3 && image(3,r,c) == 0) image.set(p,r,c,predict_and_calcProps_scanlines(properties,ranges,image,p,r,c,min,max));
                   else {
                     int oldframe=fr-1;  image.set(p,r,c,images[oldframe](p,r,c));
                     while(p == 3 && image(p,r,c) < 0) {oldframe += image(p,r,c); assert(oldframe>=0); image.set(p,r,c,images[oldframe](p,r,c));}
                   }
              } else {
                if (nump>3 && p<3) { begin=0; end=image.cols(); }
              }
              for (uint32_t c = begin; c < end; c++) {
                ColorVal guess = predict_and_calcProps_scanlines(properties,ranges,image,p,r,c,min,max);
                if (p==3 && min < -fr) min = -fr;
                if (nump>3 && p<3 && image(3,r,c) <= 0) { if (image(3,r,c) == 0) image.set(p,r,c,guess); else image.set(p,r,c,images[fr+image(3,r,c)](p,r,c)); continue;}
                ColorVal curr = coders[p]->read_int(properties, min - guess, max - guess) + guess;
                image.set(p,r,c, curr);
              }
              if (fr>0) {
                for (uint32_t c = end; c < image.cols(); c++)
                   if (nump>3 && p<3 && image(3,r,c) == 0) image.set(p,r,c,predict_and_calcProps_scanlines(properties,ranges,image,p,r,c,min,max));
                   else {
                     int oldframe=fr-1;  image.set(p,r,c,images[oldframe](p,r,c));
                     while(p == 3 && image(p,r,c) < 0) {oldframe += image(p,r,c); assert(oldframe>=0); image.set(p,r,c,images[oldframe](p,r,c));}
                   }
              }
            }
        }
    }
}

template<typename Rac, typename Coder> void decode_scanlines_pass(Rac &rac, Images &images, const ColorRanges *ranges, std::vector<Tree> &forest)
{
    std::vector<Coder*> coders;
    for (int p = 0; p < images[0].numPlanes(); p++) {
        Ranges propRanges;
        initPropRanges_scanlines(propRanges, *ranges, p);
        coders.push_back(new Coder(rac, propRanges, forest[p]));
    }
    decode_scanlines_inner(coders, images, ranges);
    for (int p = 0; p < images[0].numPlanes(); p++) {
        delete coders[p];
    }
}




/******************************************/
/*   FLIF2 encoding/decoding              */
/******************************************/
const int NB_PROPERTIES[] = {8,7,8,8};
const int NB_PROPERTIESA[] = {9,8,9,8};
void static initPropRanges(Ranges &propRanges, const ColorRanges &ranges, int p)
{
    propRanges.clear();
    int min = ranges.min(p);
    int max = ranges.max(p);
    int mind = min - max, maxd = max - min;
    if (p != 3) {       // alpha channel first
      for (int pp = 0; pp < p; pp++) {
        propRanges.push_back(std::make_pair(ranges.min(pp), ranges.max(pp)));  // pixels on previous planes
      }
      if (ranges.numPlanes()>3) propRanges.push_back(std::make_pair(ranges.min(3), ranges.max(3)));  // pixel on alpha plane
    }
    propRanges.push_back(std::make_pair(mind,maxd)); // neighbor A - neighbor B   (top-bottom or left-right)
    propRanges.push_back(std::make_pair(min,max));   // guess (median of 3)
    propRanges.push_back(std::make_pair(0,3));       // which predictor was it
    propRanges.push_back(std::make_pair(mind,maxd));
    propRanges.push_back(std::make_pair(mind,maxd));
    propRanges.push_back(std::make_pair(mind,maxd));

    if (p == 0 || p == 3) {
      propRanges.push_back(std::make_pair(mind,maxd));
      propRanges.push_back(std::make_pair(mind,maxd));
    }
}

// Prediction used for interpolation. Does not have to be the same as the guess used for encoding/decoding.
inline ColorVal predict(const Image &image, int z, int p, uint32_t r, uint32_t c)
{
    if (z%2 == 0) { // filling horizontal lines
      ColorVal top = image(p,z,r-1,c);
      ColorVal bottom = (r+1 < image.rows(z) ? image(p,z,r+1,c) : top); //grey[p]);
      ColorVal avg = (top + bottom)/2;
      return avg;
    } else { // filling vertical lines
      ColorVal left = image(p,z,r,c-1);
      ColorVal right = (c+1 < image.cols(z) ? image(p,z,r,c+1) : left); //grey[p]);
      ColorVal avg = (left + right)/2;
      return avg;
    }
}

// Actual prediction. Also sets properties. Property vector should already have the right size before calling this.
ColorVal predict_and_calcProps(Properties &properties, const ColorRanges *ranges, const Image &image, const int z, const int p, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max) {
    ColorVal guess;
    int which = 0;
    int index = 0;

    if (p != 3) {
    for (int pp = 0; pp < p; pp++) {
        properties[index++] = image(pp,z,r,c);
    }
    if (image.numPlanes()>3) properties[index++] = image(3,z,r,c);
    }
    ColorVal left;
    ColorVal top;
    ColorVal topleft = (r>0 && c>0 ? image(p,z,r-1,c-1) : grey[p]);
    ColorVal topright = (r>0 && c+1 < image.cols(z) ? image(p,z,r-1,c+1) : grey[p]);
    ColorVal bottomleft = (r+1 < image.rows(z) && c>0 ? image(p,z,r+1,c-1) : grey[p]);
    if (z%2 == 0) { // filling horizontal lines
      left = (c>0 ? image(p,z,r,c-1) : grey[p]);
      top = image(p,z,r-1,c);
      ColorVal gradientTL = left + top - topleft;
      ColorVal bottom = (r+1 < image.rows(z) ? image(p,z,r+1,c) : top); //grey[p]);
      ColorVal gradientBL = left + bottom - bottomleft;
      ColorVal avg = (top + bottom)/2;
      guess = median3(gradientTL, gradientBL, avg);
      ranges->snap(p,properties,min,max,guess);
      if (guess == avg) which = 0;
      else if (guess == gradientTL) which = 1;
      else if (guess == gradientBL) which = 2;
      properties[index++] = top-bottom;

    } else { // filling vertical lines
      left = image(p,z,r,c-1);
      top = (r>0 ? image(p,z,r-1,c) : grey[p]);
      ColorVal gradientTL = left + top - topleft;
      ColorVal right = (c+1 < image.cols(z) ? image(p,z,r,c+1) : left); //grey[p]);
      ColorVal gradientTR = right + top - topright;
      ColorVal avg = (left + right      )/2;
      guess = median3(gradientTL, gradientTR, avg);
      ranges->snap(p,properties,min,max,guess);
      if (guess == avg) which = 0;
      else if (guess == gradientTL) which = 1;
      else if (guess == gradientTR) which = 2;
      properties[index++] = left-right;
    }
    properties[index++]=guess;
    properties[index++]=which;

    if (c > 0 && r > 0) { properties[index++]=left - topleft; properties[index++]=topleft - top; }
                 else   { properties[index++]=0; properties[index++]=0; }

    if (c+1 < image.cols(z) && r > 0) properties[index++]=top - topright;
                 else   properties[index++]=0;

    if (p == 0 || p == 3) {
     if (r > 1) properties[index++]=image(p,z,r-2,c)-top;    // toptop - top
         else properties[index++]=0;
     if (c > 1) properties[index++]=image(p,z,r,c-2)-left;    // leftleft - left
         else properties[index++]=0;
    }
    return guess;
}

int plane_zoomlevels(const Image &image, const int beginZL, const int endZL) {
    return image.numPlanes() * (beginZL - endZL + 1);
}

std::pair<int, int> plane_zoomlevel(const Image &image, const int beginZL, const int endZL, int i) {
    assert(i >= 0);
    assert(i < plane_zoomlevels(image, beginZL, endZL));
    // simple order: interleave planes, zoom in
//    int p = i % image.numPlanes();
//    int zl = beginZL - (i / image.numPlanes());

    // more advanced order: give priority to more important plane(s)
    // assumption: plane 0 is Y, plane 1 is I, plane 2 is Q, plane 3 is perhaps alpha, next planes (not used at the moment) are not important
    const int max_behind[] = {0, 2, 4, 0, 16, 18, 20, 22};
    int np = image.numPlanes();
    if (np>7) {
      // too many planes, do something simple
      int p = i % image.numPlanes();
      int zl = beginZL - (i / image.numPlanes());
      return std::pair<int, int>(p,zl);
    }
    std::vector<int> czl(np);
    for (int &pzl : czl) pzl = beginZL+1;
    int highest_priority_plane = 0;
    if (np >= 4) highest_priority_plane = 3; // alpha first
    int nextp = highest_priority_plane;
    while (i >= 0) {
      czl[nextp]--;
      i--;
      if (i<0) break;
      nextp=highest_priority_plane;
      for (int p=0; p<np; p++) {
        if (czl[p] > czl[highest_priority_plane] + max_behind[p]) {
          nextp = p; break;
        }
      }
      // ensure that nextp is not at the most detailed zoomlevel yet
      while (czl[nextp] <= endZL) nextp = (nextp+1)%np;
    }
    int p = nextp;
    int zl = czl[p];

    return std::pair<int, int>(p,zl);
}

template<typename Coder> void encode_FLIF2_inner(std::vector<Coder*> &coders, const Images &images, const ColorRanges *ranges, const int beginZL, const int endZL)
{
    ColorVal min,max;
    int nump = images[0].numPlanes();
    long fs = ftell(f);
    for (int i = 0; i < plane_zoomlevels(images[0], beginZL, endZL); i++) {
      std::pair<int, int> pzl = plane_zoomlevel(images[0], beginZL, endZL, i);
      int p = pzl.first;
      int z = pzl.second;
      if (endZL==0) {
          v_printf(2,"\r%i%% done [%i/%i] ENC[%i,%ux%u]  ",(int) (100*pixels_done/pixels_todo),i,plane_zoomlevels(images[0], beginZL, endZL)-1,p,images[0].cols(z),images[0].rows(z));
      }
      pixels_done += images[0].cols(z)*images[0].rows(z)/2;
      if (ranges->min(p) >= ranges->max(p)) continue;
      Properties properties((nump>3?NB_PROPERTIESA[p]:NB_PROPERTIES[p]));
      if (z % 2 == 0) {
        // horizontal: scan the odd rows, output pixel values
          for (uint32_t r = 1; r < images[0].rows(z); r += 2) {
            for (int fr=0; fr<(int)images.size(); fr++) {
              const Image& image = images[fr];
              if (image.seen_before >= 0) { continue; }
              uint32_t begin=(image.col_begin[r*image.zoom_rowpixelsize(z)]/image.zoom_colpixelsize(z)),
                         end=(1+(image.col_end[r*image.zoom_rowpixelsize(z)]-1)/image.zoom_colpixelsize(z));
              for (uint32_t c = begin; c < end; c++) {
                    if (nump>3 && p<3 && image(3,z,r,c) <= 0) continue;
                    ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max);
                    ColorVal curr = image(p,z,r,c);
                    if (p==3 && min < -fr) min = -fr;
                    assert (curr <= max); assert (curr >= min);
                    coders[p]->write_int(properties, min - guess, max - guess, curr - guess);
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
                    if (nump>3 && p<3 && image(3,z,r,c) <= 0) continue;
                    ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max);
                    ColorVal curr = image(p,z,r,c);
                    if (p==3 && min < -fr) min = -fr;
                    assert (curr <= max); assert (curr >= min);
                    coders[p]->write_int(properties, min - guess, max - guess, curr - guess);
              }
            }
          }
      }
      if (endZL==0 && ftell(f)>fs) {
          v_printf(3,"    wrote %li bytes    ", ftell(f));
          v_printf(5,"\n");
          fs = ftell(f);
      }
    }
}

template<typename Rac, typename Coder> void encode_FLIF2_pass(Rac &rac, const Images &images, const ColorRanges *ranges, std::vector<Tree> &forest, const int beginZL, const int endZL, int repeats)
{
    std::vector<Coder*> coders;
    for (int p = 0; p < ranges->numPlanes(); p++) {
        Ranges propRanges;
        initPropRanges(propRanges, *ranges, p);
        coders.push_back(new Coder(rac, propRanges, forest[p]));
    }

    for (const Image& image : images)
    if (beginZL == image.zooms()) {
      // special case: very left top pixel must be written first to get it all started
      SimpleSymbolCoder<FLIFBitChanceMeta, Rac, 24> metaCoder(rac);
      for (int p = 0; p < image.numPlanes(); p++) {
        ColorVal curr = image(p,0,0);
        metaCoder.write_int(ranges->min(p), ranges->max(p), curr);
      }
    }
    while(repeats-- > 0) {
     encode_FLIF2_inner(coders, images, ranges, beginZL, endZL);
    }
    for (int p = 0; p < images[0].numPlanes(); p++) {
        coders[p]->simplify();
    }

    for (int p = 0; p < images[0].numPlanes(); p++) {
#ifdef STATS
        indent(0); v_printf(2,"Plane %i\n", p);
        coders[p]->info(0+1);
#endif
        delete coders[p];
    }
}

void encode_FLIF2_interpol_zero_alpha(Images &images, const ColorRanges *ranges, const int beginZL, const int endZL)
{
    for (Image& image : images)
    for (int i = 0; i < plane_zoomlevels(image, beginZL, endZL); i++) {
      std::pair<int, int> pzl = plane_zoomlevel(image, beginZL, endZL, i);
      int p = pzl.first;
      int z = pzl.second;
      if (p == 3) continue;
//      v_printf(2,"[%i] interpol_zero_alpha ",p);
//      fflush(stdout);
      if (z % 2 == 0) {
        // horizontal: scan the odd rows
          for (uint32_t r = 1; r < image.rows(z); r += 2) {
            for (uint32_t c = 0; c < image.cols(z); c++) {
               if (image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c));
            }
          }
      } else {
        // vertical: scan the odd columns
          for (uint32_t r = 0; r < image.rows(z); r++) {
            for (uint32_t c = 1; c < image.cols(z); c += 2) {
               if (image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c));
            }
          }
      }
    }
//    v_printf(2,"\n");
}


// interpolate rest of the image
// used when decoding lossy
void decode_FLIF2_inner_interpol(Images &images, const ColorRanges *ranges, const int I, const int beginZL, const int endZL, const uint32_t R, const int scale)
{
    for (int i = I; i < plane_zoomlevels(images[0], beginZL, endZL); i++) {
      std::pair<int, int> pzl = plane_zoomlevel(images[0], beginZL, endZL, i);
      int p = pzl.first;
      int z = pzl.second;
      if ( 1<<(z/2) < scale) continue;
      pixels_done += images[0].cols(z)*images[0].rows(z)/2;
      v_printf(2,"\r%i%% done [%i/%i] INTERPOLATE[%i,%ux%u]                 ",(int)(100*pixels_done/pixels_todo),i,plane_zoomlevels(images[0], beginZL, endZL)-1,p,images[0].cols(z),images[0].rows(z));
      v_printf(5,"\n");

      if (z % 2 == 0) {
        // horizontal: scan the odd rows
          for (uint32_t r = (I==i?R:1); r < images[0].rows(z); r += 2) {
            for (Image& image : images) {
              if (image.palette == false) {
               for (uint32_t c = 0; c < image.cols(z); c++) {
                 image.set(p,z,r,c, predict(image,z,p,r,c));    // normal method: use predict() for interpolation
               }
              } else {
               for (uint32_t c = 0; c < image.cols(z); c++) {
                 image.set(p,z,r,c, image(p,z,r-1,c));          // paletted image: no interpolation
               }
              }
            }
          }
      } else {
        // vertical: scan the odd columns
          for (uint32_t r = (I==i?R:0); r < images[0].rows(z); r++) {
            for (Image& image : images) {
              if (image.palette == false) {
               for (uint32_t c = 1; c < image.cols(z); c += 2) {
                image.set(p,z,r,c, predict(image,z,p,r,c));
               }
              } else {
               for (uint32_t c = 1; c < image.cols(z); c += 2) {
                image.set(p,z,r,c, image(p,z,r,c-1));
               }
              }
            }
          }
      }
    }
    v_printf(2,"\n");
}

template<typename Coder> void decode_FLIF2_inner(std::vector<Coder*> &coders, Images &images, const ColorRanges *ranges, const int beginZL, const int endZL, int quality, int scale)
{
    ColorVal min,max;
    int nump = images[0].numPlanes();
//    if (quality >= 0) {
//      quality = plane_zoomlevels(image, beginZL, endZL) * quality / 100;
//    }
    // decode
    for (int i = 0; i < plane_zoomlevels(images[0], beginZL, endZL); i++) {
      std::pair<int, int> pzl = plane_zoomlevel(images[0], beginZL, endZL, i);
      int p = pzl.first;
      int z = pzl.second;
      if ((100*pixels_done > quality*pixels_todo) ||  1<<(z/2) < scale) {
              decode_FLIF2_inner_interpol(images, ranges, i, beginZL, endZL, (z%2 == 0 ?1:0), scale);
              return;
      }
      if (endZL == 0) v_printf(2,"\r%i%% done [%i/%i] DEC[%i,%ux%u]  ",(int)(100*pixels_done/pixels_todo),i,plane_zoomlevels(images[0], beginZL, endZL)-1,p,images[0].cols(z),images[0].rows(z));
      pixels_done += images[0].cols(z)*images[0].rows(z)/2;
      if (ranges->min(p) >= ranges->max(p)) continue;
      ColorVal curr;
      Properties properties((nump>3?NB_PROPERTIESA[p]:NB_PROPERTIES[p]));
      if (z % 2 == 0) {
          for (uint32_t r = 1; r < images[0].rows(z); r += 2) {
#ifdef CHECK_FOR_BROKENFILES
            if (feof(f)) {
              v_printf(1,"Row %i: Unexpected file end. Interpolation from now on.\n",r);
              decode_FLIF2_inner_interpol(images, ranges, i, beginZL, endZL, (r>1?r-2:r), scale);
              return;
            }
#endif
            for (int fr=0; fr<(int)images.size(); fr++) {
              Image& image = images[fr];
              if (image.seen_before >= 0) { for (uint32_t c=0; c<image.cols(z); c++) image.set(p,z,r,c,images[image.seen_before](p,z,r,c)); continue; }
              uint32_t begin=image.col_begin[r*image.zoom_rowpixelsize(z)]/image.zoom_colpixelsize(z), end=1+(image.col_end[r*image.zoom_rowpixelsize(z)]-1)/image.zoom_colpixelsize(z);
              if (fr>0) {
                for (uint32_t c = 0; c < begin; c++)
                            if (nump>3 && p<3 && image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c));
                            else { int oldframe=fr-1;  image.set(p,z,r,c,images[oldframe](p,z,r,c));
                                   while(p == 3 && image(p,z,r,c) < 0) {oldframe += image(p,z,r,c); assert(oldframe>=0); image.set(p,z,r,c,images[oldframe](p,z,r,c));}}
                for (uint32_t c = end; c < image.cols(z); c++)
                            if (nump>3 && p<3 && image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c));
                            else { int oldframe=fr-1;  image.set(p,z,r,c,images[oldframe](p,z,r,c));
                                   while(p == 3 && image(p,z,r,c) < 0) {oldframe += image(p,z,r,c); assert(oldframe>=0); image.set(p,z,r,c,images[oldframe](p,z,r,c));}}
              } else {
                if (nump>3 && p<3) { begin=0; end=image.cols(z); }
              }
              for (uint32_t c = begin; c < end; c++) {
                     if (nump>3 && p<3 && image(3,z,r,c) <= 0) { if (image(3,z,r,c) == 0) image.set(p,z,r,c,predict(image,z,p,r,c)); else image.set(p,z,r,c,images[fr+image(3,z,r,c)](p,z,r,c)); continue;}
                     ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max);
                     if (p==3 && min < -fr) min = -fr;
                     curr = coders[p]->read_int(properties, min - guess, max - guess) + guess;
                     image.set(p,z,r,c, curr);
              }
            }
        }
      } else {
          for (uint32_t r = 0; r < images[0].rows(z); r++) {
#ifdef CHECK_FOR_BROKENFILES
            if (feof(f)) {
              v_printf(1,"Row %i: Unexpected file end. Interpolation from now on.\n", r);
              decode_FLIF2_inner_interpol(images, ranges, i, beginZL, endZL, (r>0?r-1:r), scale);
              return;
            }
#endif
            for (int fr=0; fr<(int)images.size(); fr++) {
              Image& image = images[fr];
              if (image.seen_before >= 0) { for (uint32_t c=1; c<image.cols(z); c+=2) image.set(p,z,r,c,images[image.seen_before](p,z,r,c)); continue; }
              uint32_t begin=(image.col_begin[r*image.zoom_rowpixelsize(z)]/image.zoom_colpixelsize(z)),
              end=(1+(image.col_end[r*image.zoom_rowpixelsize(z)]-1)/image.zoom_colpixelsize(z))|1;
              if (begin>1 && ((begin&1) ==0)) begin--;
              if (begin==0) begin=1;
              if (fr>0) {
                for (uint32_t c = 1; c < begin; c+=2)
                     if (nump>3 && p<3 && image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c));
                     else { int oldframe=fr-1;  image.set(p,z,r,c,images[oldframe](p,z,r,c));
                            while(p == 3 && image(p,z,r,c) < 0) {oldframe += image(p,z,r,c); assert(oldframe>=0); image.set(p,z,r,c,images[oldframe](p,z,r,c));}}
                for (uint32_t c = end; c < image.cols(z); c+=2)
                     if (nump>3 && p<3 && image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c));
                     else { int oldframe=fr-1;  image.set(p,z,r,c,images[oldframe](p,z,r,c));
                            while(p == 3 && image(p,z,r,c) < 0) {oldframe += image(p,z,r,c); assert(oldframe>=0); image.set(p,z,r,c,images[oldframe](p,z,r,c));}}
              } else {
                if (nump>3 && p<3) { begin=1; end=image.cols(z); }
              }
              for (uint32_t c = begin; c < end; c+=2) {
                     if (nump>3 && p<3 && image(3,z,r,c) <= 0) { if (image(3,z,r,c) == 0) image.set(p,z,r,c,predict(image,z,p,r,c)); else image.set(p,z,r,c,images[fr+image(3,z,r,c)](p,z,r,c)); continue;}
                     ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max);
                     if (p==3 && min < -fr) min = -fr;
                     curr = coders[p]->read_int(properties, min - guess, max - guess) + guess;
                     image.set(p,z,r,c, curr);
              }
            }
        }
      }
      if (endZL==0) {
          v_printf(3,"    read %li bytes   ", ftell(f));
          v_printf(5,"\n");
      }
    }
}

template<typename Rac, typename Coder> void decode_FLIF2_pass(Rac &rac, Images &images, const ColorRanges *ranges, std::vector<Tree> &forest, const int beginZL, const int endZL, int quality, int scale)
{
    std::vector<Coder*> coders;
    for (int p = 0; p < images[0].numPlanes(); p++) {
        Ranges propRanges;
        initPropRanges(propRanges, *ranges, p);
        coders.push_back(new Coder(rac, propRanges, forest[p]));
    }

    for (Image& image : images)
    if (beginZL == image.zooms()) {
      // special case: very left top pixel must be read first to get it all started
      SimpleSymbolCoder<FLIFBitChanceMeta, Rac, 24> metaCoder(rac);
      for (int p = 0; p < image.numPlanes(); p++) {
        image.set(p,0,0, metaCoder.read_int(ranges->min(p), ranges->max(p)));
      }
    }

    decode_FLIF2_inner(coders, images, ranges, beginZL, endZL, quality, scale);

    for (int p = 0; p < images[0].numPlanes(); p++) {
        delete coders[p];
    }
}


/******************************************/
/*   General encoding/decoding            */
/******************************************/

template<typename BitChance, typename Rac> void encode_tree(Rac &rac, const ColorRanges *ranges, const std::vector<Tree> &forest, const int encoding)
{
    for (int p = 0; p < ranges->numPlanes(); p++) {
        Ranges propRanges;
        if (encoding==1) initPropRanges_scanlines(propRanges, *ranges, p);
        else initPropRanges(propRanges, *ranges, p);
        MetaPropertySymbolCoder<BitChance, Rac> metacoder(rac, propRanges);
//        forest[p].print(stdout);
        if (ranges->min(p)<ranges->max(p))
        metacoder.write_tree(forest[p]);
    }
}
template<typename BitChance, typename Rac> void decode_tree(Rac &rac, const ColorRanges *ranges, std::vector<Tree> &forest, const int encoding)
{
    for (int p = 0; p < ranges->numPlanes(); p++) {
        Ranges propRanges;
        if (encoding==1) initPropRanges_scanlines(propRanges, *ranges, p);
        else initPropRanges(propRanges, *ranges, p);
        MetaPropertySymbolCoder<BitChance, Rac> metacoder(rac, propRanges);
        if (ranges->min(p)<ranges->max(p))
        metacoder.read_tree(forest[p]);
//        forest[p].print(stdout);
    }
}


bool encode(const char* filename, Images &images, std::vector<std::string> transDesc, int encoding, int learn_repeats, int acb, int frame_delay, int palette_size, int lookback) {
    if (encoding < 1 || encoding > 2) { fprintf(stderr,"Unknown encoding: %i\n", encoding); return false;}
    f = fopen(filename,"wb");
    fputs("FLIF",f);
    int numPlanes = images[0].numPlanes();
    int numFrames = images.size();
    char c=' '+16*encoding+numPlanes;
    if (numFrames>1) c += 32;
    fputc(c,f);
    if (numFrames>1) {
        if (numFrames<255) fputc((char)numFrames,f);
        else {
            fprintf(stderr,"Too many frames!\n");
        }
    }
    c='1';
    for (int p = 0; p < numPlanes; p++) {if (images[0].max(p) != 255) c='2';}
    if (c=='2') {for (int p = 0; p < numPlanes; p++) {if (images[0].max(p) != 65535) c='0';}}
    fputc(c,f);

    Image& image = images[0];
    assert(image.cols() <= 0xFFFF);
    fputc(image.cols() >> 8,f);
    fputc(image.cols() & 0xFF,f);
    assert(image.rows() <= 0xFFFF);
    fputc(image.rows() >> 8,f);
    fputc(image.rows() & 0xFF,f);

    RacOut rac(f);
    SimpleSymbolCoder<FLIFBitChanceMeta, RacOut, 24> metaCoder(rac);

    v_printf(3,"Input: %ux%u, channels:", images[0].cols(), images[0].rows());
    for (int p = 0; p < numPlanes; p++) {
        assert(image.min(p) == 0);
        if (c=='0') {
          metaCoder.write_int(1, 16, ilog2(image.max(p)+1));
          v_printf(3," [%i] %i bpp",p,ilog2(image.max(p)+1));
        }
    }
    if (c=='1') v_printf(3," %i, depth: 8 bit",numPlanes);
    if (c=='2') v_printf(3," %i, depth: 16 bit",numPlanes);
    if (numFrames>1) v_printf(3,", frames: %i",numFrames);
    v_printf(3,"\n");
    if (numFrames>1) {
        for (int i=0; i<numFrames; i++) {
           metaCoder.write_int(0, 60000, frame_delay); // time in ms between frames
        }
    }
//    metaCoder.write_int(1, 65536, image.cols());
//    metaCoder.write_int(1, 65536, image.rows());
//    v_printf(2,"Header: %li bytes.\n", ftell(f));

//    v_printf(2,"Header: %li bytes.\n", ftell(f));

    std::vector<const ColorRanges*> rangesList;
    std::vector<Transform*> transforms;
    rangesList.push_back(getRanges(image));
    int tcount=0;
    v_printf(4,"Transforms: ");
    for (unsigned int i=0; i<transDesc.size(); i++) {
        Transform *trans = create_transform(transDesc[i]);
        if (transDesc[i] == "PLT" || transDesc[i] == "PLA") trans->configure(palette_size);
        if (transDesc[i] == "FRA") trans->configure(lookback);
        if (!trans->init(rangesList.back()) || 
            (!trans->process(rangesList.back(), images)
              && !(acb==1 && transDesc[i] == "ACB" && printf(", forced_") && (tcount=0)==0))) {
            //fprintf(stderr, "Transform '%s' failed\n", transDesc[i].c_str());
        } else {
            if (tcount++ > 0) v_printf(4,", ");
            v_printf(4,"%s", transDesc[i].c_str());
            fflush(stdout);
            rac.write(true);
            write_name(rac, transDesc[i]);
            trans->save(rangesList.back(), rac);
            fflush(stdout);
            rangesList.push_back(trans->meta(images, rangesList.back()));
            trans->data(images);
        }
        delete trans;
    }
    if (tcount==0) v_printf(4,"none\n"); else v_printf(4,"\n");
    rac.write(false);
    const ColorRanges* ranges = rangesList.back();
    grey.clear();
    for (int p = 0; p < ranges->numPlanes(); p++) grey.push_back((ranges->min(p)+ranges->max(p))/2);

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
    int bits = 10; // hardcoding things for 8 bit RGB (which means 9 bit IQ and 10 bit differences)
    if (mbits >10) bits=18;
    if (mbits > bits) { printf("OOPS: %i > %i\n",mbits,bits); return false;}

    pixels_todo = image.rows()*image.cols()*ranges->numPlanes()*(learn_repeats+1);

    // two passes
    std::vector<Tree> forest(ranges->numPlanes(), Tree());
    RacDummy dummy;

    if (ranges->numPlanes() > 3) {
      v_printf(4,"Replacing fully transparent pixels with predicted pixel values at the other planes\n");
      switch(encoding) {
        case 1: encode_scanlines_interpol_zero_alpha(images, ranges); break;
        case 2: encode_FLIF2_interpol_zero_alpha(images, ranges, image.zooms(), 0); break;
      }
    }

    // not computing checksum until after transformations and potential zero-alpha changes
    uint32_t checksum = image.checksum();
    long fs = ftell(f);

    int roughZL = 0;
    if (encoding == 2) {
      roughZL = image.zooms() - NB_NOLEARN_ZOOMS-1;
      if (roughZL < 0) roughZL = 0;
      //v_printf(2,"Encoding rough data\n");
      if (bits==10) encode_FLIF2_pass<RacOut, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut, 10> >(rac, images, ranges, forest, image.zooms(), roughZL+1, 1);
      else encode_FLIF2_pass<RacOut, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut, 18> >(rac, images, ranges, forest, image.zooms(), roughZL+1, 1);
    }

    //v_printf(2,"Encoding data (pass 1)\n");
    if (learn_repeats>1) v_printf(3,"Learning a MANIAC tree. Iterating %i times.\n",learn_repeats);
    switch(encoding) {
        case 1:
           if (bits==10) encode_scanlines_pass<RacDummy, PropertySymbolCoder<FLIFBitChancePass1, RacDummy, 10> >(dummy, images, ranges, forest, learn_repeats);
           else encode_scanlines_pass<RacDummy, PropertySymbolCoder<FLIFBitChancePass1, RacDummy, 18> >(dummy, images, ranges, forest, learn_repeats);
           break;
        case 2:
           if (bits==10) encode_FLIF2_pass<RacDummy, PropertySymbolCoder<FLIFBitChancePass1, RacDummy, 10> >(dummy, images, ranges, forest, roughZL, 0, learn_repeats);
           else encode_FLIF2_pass<RacDummy, PropertySymbolCoder<FLIFBitChancePass1, RacDummy, 18> >(dummy, images, ranges, forest, roughZL, 0, learn_repeats);
           break;
    }
    v_printf(3,"\rHeader: %li bytes.", fs);
    if (encoding==2) v_printf(3," Rough data: %li bytes.", ftell(f)-fs);
    fflush(stdout);

    //v_printf(2,"Encoding tree\n");
    fs = ftell(f);
    encode_tree<FLIFBitChanceTree, RacOut>(rac, ranges, forest, encoding);
    v_printf(3," MANIAC tree: %li bytes.\n", ftell(f)-fs);
    //v_printf(2,"Encoding data (pass 2)\n");
    switch(encoding) {
        case 1:
           if (bits==10) encode_scanlines_pass<RacOut, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut, 10> >(rac, images, ranges, forest, 1);
           else encode_scanlines_pass<RacOut, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut, 18> >(rac, images, ranges, forest, 1);
           break;
        case 2:
           if (bits==10) encode_FLIF2_pass<RacOut, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut, 10> >(rac, images, ranges, forest, roughZL, 0, 1);
           else encode_FLIF2_pass<RacOut, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut, 18> >(rac, images, ranges, forest, roughZL, 0, 1);
           break;
    }
    if (numFrames==1)
      v_printf(2,"\rEncoding done, %li bytes for %ux%u pixels (%.4fbpp)   \n",ftell(f), images[0].cols(), images[0].rows(), 1.0*ftell(f)/images[0].rows()/images[0].cols());
    else
      v_printf(2,"\rEncoding done, %li bytes for %i frames of %ux%u pixels (%.4fbpp)   \n",ftell(f), numFrames, images[0].cols(), images[0].rows(), 1.0*ftell(f)/numFrames/images[0].rows()/images[0].cols());

    //v_printf(2,"Writing checksum: %X\n", checksum);
    metaCoder.write_int(0, 0xFFFF, checksum / 0x10000);
    metaCoder.write_int(0, 0xFFFF, checksum & 0xFFFF);
    rac.flush();
    fclose(f);

    for (int i=transforms.size()-1; i>=0; i--) {
        delete transforms[i];
    }
    transforms.clear();
    for (unsigned int i=0; i<rangesList.size(); i++) {
        delete rangesList[i];
    }
    rangesList.clear();
    return true;
}



bool decode(const char* filename, Images &images, int quality, int scale)
{
    if (scale != 1 && scale != 2 && scale != 4 && scale != 8 && scale != 16 && scale != 32 && scale != 64 && scale != 128) {
                fprintf(stderr,"Invalid scale down factor: %i\n", scale);
                return false;
    }

    f = fopen(filename,"rb");
    if (!f) { fprintf(stderr,"Could not open file: %s\n",filename); return false; }
    char buff[5];
    if (!fgets(buff,5,f)) { fprintf(stderr,"Could not read header from file: %s\n",filename); return false; }
    if (strcmp(buff,"FLIF")) { fprintf(stderr,"Not a FLIF file: %s\n",filename); return false; }
    int c = fgetc(f)-' ';
    int numFrames=1;
    if (c > 47) {
        c -= 32;
        numFrames = fgetc(f);
    }
    int encoding=c/16;
    if (scale != 1 && encoding==1) { v_printf(1,"Cannot decode non-interlaced FLIF file at lower scale! Ignoring scale...\n");}
    if (quality < 100 && encoding==1) { v_printf(1,"Cannot decode non-interlaced FLIF file at lower quality! Ignoring quality...\n");}
    int numPlanes=c%16;
    c = fgetc(f);

    int width=fgetc(f) << 8;
    width += fgetc(f);
    int height=fgetc(f) << 8;
    height += fgetc(f);
    // TODO: implement downscaled decoding without allocating a fullscale image buffer!

    RacIn rac(f);
    SimpleSymbolCoder<FLIFBitChanceMeta, RacIn, 24> metaCoder(rac);

//    image.init(width, height, 0, 0, 0);
    v_printf(3,"Decoding %ux%u image, channels:",width,height);
    int maxmax=0;
    for (int p = 0; p < numPlanes; p++) {
//        int min = 0;
        int max = 255;
        if (c=='2') max=65535;
        else if (c=='0') max=(1 << metaCoder.read_int(1, 16)) - 1;
        if (max>maxmax) maxmax=max;
//        image.add_plane(min, max);
//        v_printf(2," [%i] %i bpp (%i..%i)",p,ilog2(image.max(p)+1),image.min(p), image.max(p));
        if (c=='0') v_printf(3," [%i] %i bpp",p,ilog2(max+1));
    }
    if (c=='1') v_printf(3," %i, depth: 8 bit",numPlanes);
    if (c=='2') v_printf(3," %i, depth: 16 bit",numPlanes);
    if (numFrames>1) v_printf(3,", frames: %i",numFrames);
    v_printf(3,"\n");

    if (numFrames>1) {
        for (int i=0; i<numFrames; i++) {
           metaCoder.read_int(0, 60000); // time in ms between frames
        }
    }

    for (int i=0; i<numFrames; i++) {
      Image image;
      images.push_back(image);
      images[i].init(width,height,0,maxmax,numPlanes);
    }
    std::vector<const ColorRanges*> rangesList;
    std::vector<Transform*> transforms;
    rangesList.push_back(getRanges(images[0]));
    v_printf(4,"Transforms: ");
    int tcount=0;
    while (rac.read()) {
        std::string desc = read_name(rac);
        Transform *trans = create_transform(desc);
        if (!trans) {
            fprintf(stderr,"Unknown transformation '%s'\n", desc.c_str());
            return false;
        }
        if (!trans->init(rangesList.back())) {
            fprintf(stderr,"Transformation '%s' failed\n", desc.c_str());
            return false;
        }
        if (tcount++ > 0) v_printf(4,", ");
        v_printf(4,"%s", desc.c_str());
        if (desc == "FRS") {
                int unique_frames=images.size()-1; // not considering first frame
                for (Image& i : images) if (i.seen_before >= 0) unique_frames--;
                trans->configure(unique_frames*images[0].rows()); trans->configure(images[0].cols()); }
        if (desc == "DUP") { trans->configure(images.size()); }
        trans->load(rangesList.back(), rac);
        rangesList.push_back(trans->meta(images, rangesList.back()));
        transforms.push_back(trans);
    }
    if (tcount==0) v_printf(4,"none\n"); else v_printf(4,"\n");
    const ColorRanges* ranges = rangesList.back();
    grey.clear();
    for (int p = 0; p < ranges->numPlanes(); p++) grey.push_back((ranges->min(p)+ranges->max(p))/2);

    pixels_todo = width*height*ranges->numPlanes()/scale/scale;

    for (int p = 0; p < ranges->numPlanes(); p++) {
      v_printf(7,"Plane %i: %i..%i\n",p,ranges->min(p),ranges->max(p));
    }

    for (int p = 0; p < ranges->numPlanes(); p++) {
        if (ranges->min(p) >= ranges->max(p)) {
             v_printf(4,"Constant plane %i at color value %i\n",p,ranges->min(p));
             //for (ColorVal_intern& x : image(p).data) x=ranges->min(p);
            for (int fr = 0; fr < numFrames; fr++)
            for (uint32_t r=0; r<images[fr].rows(); r++) {
              for (uint32_t c=0; c<images[fr].cols(); c++) {
                images[fr].set(p,r,c,ranges->min(p));
              }
            }
        }
    }
    int mbits = 0;
    for (int p = 0; p < ranges->numPlanes(); p++) {
        if (ranges->max(p) > ranges->min(p)) {
          int nBits = ilog2((ranges->max(p) - ranges->min(p))*2-1)+1;
          if (nBits > mbits) mbits = nBits;
        }
    }
    int bits = 10;
    if (mbits >10) bits=18;
    if (mbits > bits) { printf("OOPS: %i > %i\n",mbits,bits); return false;}


    std::vector<Tree> forest(ranges->numPlanes(), Tree());

    int roughZL = 0;
    if (encoding == 2) {
      roughZL = images[0].zooms() - NB_NOLEARN_ZOOMS-1;
      if (roughZL < 0) roughZL = 0;
//      v_printf(2,"Decoding rough data\n");
      if (bits==10) decode_FLIF2_pass<RacIn, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn, 10> >(rac, images, ranges, forest, images[0].zooms(), roughZL+1, 100, scale);
      else decode_FLIF2_pass<RacIn, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn, 18> >(rac, images, ranges, forest, images[0].zooms(), roughZL+1, 100, scale);
    }
    if (encoding == 2 && quality <= 0) {
      v_printf(3,"Not decoding MANIAC tree\n");
    } else {
      v_printf(3,"Decoded header + rough data. Decoding MANIAC tree.\n");
      decode_tree<FLIFBitChanceTree, RacIn>(rac, ranges, forest, encoding);
    }
//    if (encoding == 1 || quality > 0) {
      switch(encoding) {
        case 1: v_printf(3,"Decoding data (scanlines)\n");
                if (bits==10) decode_scanlines_pass<RacIn, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn, 10> >(rac, images, ranges, forest);
                else decode_scanlines_pass<RacIn, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn, 18> >(rac, images, ranges, forest);
                break;
        case 2: v_printf(3,"Decoding data (FLIF2)\n");
                if (bits==10) decode_FLIF2_pass<RacIn, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn, 10> >(rac, images, ranges, forest, roughZL, 0, quality, scale);
                else decode_FLIF2_pass<RacIn, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn, 18> >(rac, images, ranges, forest, roughZL, 0, quality, scale);
                break;
      }
//    }
    if (numFrames==1)
      v_printf(2,"\rDecoding done, %li bytes for %ux%u pixels (%.4fbpp)   \n",ftell(f), images[0].cols()/scale, images[0].rows()/scale, 1.0*ftell(f)/images[0].rows()/images[0].cols()/scale/scale);
    else
      v_printf(2,"\rDecoding done, %li bytes for %i frames of %ux%u pixels (%.4fbpp)   \n",ftell(f), numFrames, images[0].cols()/scale, images[0].rows()/scale, 1.0*ftell(f)/numFrames/images[0].rows()/images[0].cols()/scale/scale);


    if (quality==100 && scale==1) {
      uint32_t checksum = images[0].checksum();
      v_printf(8,"Computed checksum: %X\n", checksum);
      uint32_t checksum2 = metaCoder.read_int(0, 0xFFFF);
      checksum2 *= 0x10000;
      checksum2 += metaCoder.read_int(0, 0xFFFF);
      v_printf(8,"Read checksum: %X\n", checksum2);
      if (checksum != checksum2) v_printf(1,"\nCORRUPTION DETECTED! (partial file?)\n\n");
      else v_printf(2,"Image decoded, checksum verified.\n");
    } else {
      v_printf(2,"Not checking checksum, lossy partial decoding was chosen.\n");
    }

    for (int i=transforms.size()-1; i>=0; i--) {
        transforms[i]->invData(images);
        delete transforms[i];
    }
    transforms.clear();


    for (unsigned int i=0; i<rangesList.size(); i++) {
        delete rangesList[i];
    }
    rangesList.clear();

    fclose(f);
    return true;
}


void show_help() {
    printf("Usage: (encoding)\n");
    printf("   flif [encode options] <input image(s)> <output.flif>\n");
    printf("   flif [-d] [decode options] <input.flif> <output.pnm | output.pam | output.png>\n");
    printf("General Options:\n");
    printf("   -h, --help           show help\n");
    printf("   -v, --verbose        increase verbosity (multiple -v for more output)\n");
    printf("Encode options:\n");
    printf("   -i, --interlace      interlacing (default, except for tiny images)\n");
    printf("   -n, --no-interlace   force no interlacing\n");
    printf("   -a, --acb            force auto color buckets (ACB)\n");
    printf("   -b, --no-acb         force no auto color buckets\n");
    printf("   -p, --palette=P      max palette size=P (default: P=512)\n");
    printf("   -r, --repeats=N      N repeats for MANIAC learning (default: N=%i)\n",TREE_LEARN_REPEATS);
    printf("   Input images should be PNG, PNM (PPM,PGM,PBM) or PAM files.\n");
    printf("   Multiple input images (for animated FLIF) must have the same dimensions.\n");
    printf("   -f, --frame-delay=D  delay between animation frames, in ms (default: D=100)\n");
    printf("   -l, --lookback=L     max lookback between frames (default: L=1)\n");
    printf("Decode options:\n");
    printf("   -q, --quality=Q      lossy decode quality at Q percent (0..100)\n");
    printf("   -s, --scale=S        lossy downscaled image at scale 1:S (2,4,8,16)\n");
}

bool file_exists(const char * filename){
        FILE * file = fopen(filename, "rb");
        if (!file) return false;
        fclose(file);
        return true;
}
bool file_is_flif(const char * filename){
        FILE * file = fopen(filename, "rb");
        if (!file) return false;
        char buff[5];
        bool result=true;
        if (!fgets(buff,5,file)) result=false;
        else if (strcmp(buff,"FLIF")) result=false;
        fclose(file);
        return result;
}

int main(int argc, char **argv)
{
    Images images;
    int mode = 0; // 0 = encode, 1 = decode
    int method = 0; // 1=non-interlacing, 2=interlacing
    int quality = 100; // 100 = everything, positive value: partial decode, negative value: only rough data
    int learn_repeats = -1;
    int acb = -1; // try auto color buckets
    int scale = 1;
    int frame_delay = 100;
    int palette_size = 512;
    int lookback = 1;
    if (strcmp(argv[0],"flif") == 0) mode = 0;
    if (strcmp(argv[0],"dflif") == 0) mode = 1;
    if (strcmp(argv[0],"deflif") == 0) mode = 1;
    if (strcmp(argv[0],"decflif") == 0) mode = 1;
    static struct option optlist[] = {
        {"help", 0, NULL, 'h'},
        {"encode", 0, NULL, 'e'},
        {"decode", 0, NULL, 'd'},
        {"first", 1, NULL, 'f'},
        {"verbose", 0, NULL, 'v'},
        {"interlace", 0, NULL, 'i'},
        {"no-interlace", 0, NULL, 'n'},
        {"acb", 0, NULL, 'a'},
        {"no-acb", 0, NULL, 'b'},
        {"quality", 1, NULL, 'q'},
        {"scale", 1, NULL, 's'},
        {"palette", 1, NULL, 'p'},
        {"repeats", 1, NULL, 'r'},
        {"frame-delay", 1, NULL, 'f'},
        {"lookback", 1, NULL, 'l'},
        {0, 0, 0, 0}
    };
    int i,c;
    while ((c = getopt_long (argc, argv, "hedvinabq:s:p:r:f:l:", optlist, &i)) != -1) {
        switch (c) {
        case 'e': mode=0; break;
        case 'd': mode=1; break;
        case 'v': verbosity++; break;
        case 'i': if (method==0) method=2; break;
        case 'n': method=1; break;
        case 'a': acb=1; break;
        case 'b': acb=0; break;
        case 'p': palette_size=atoi(optarg);
                  if (palette_size < -1 || palette_size > 30000) {fprintf(stderr,"Not a sensible number for option -p\n"); return 1; }
                  if (palette_size == 0) {v_printf(2,"Palette disabled\n"); }
                  break;
        case 'q': quality=atoi(optarg);
                  if (quality < -1 || quality > 100) {fprintf(stderr,"Not a sensible number for option -q\n"); return 1; }
                  break;
        case 's': scale=atoi(optarg);
                  if (scale < 1 || scale > 128) {fprintf(stderr,"Not a sensible number for option -s\n"); return 1; }
                  break;
        case 'r': learn_repeats=atoi(optarg);
                  if (learn_repeats < 0 || learn_repeats > 1000) {fprintf(stderr,"Not a sensible number for option -r\n"); return 1; }
                  break;
        case 'f': frame_delay=atoi(optarg);
                  if (frame_delay < 0 || frame_delay > 60000) {fprintf(stderr,"Not a sensible number for option -f\n"); return 1; }
                  break;
        case 'l': lookback=atoi(optarg);
                  if (lookback < -1 || lookback > 256) {fprintf(stderr,"Not a sensible number for option -l\n"); return 1; }
                  break;
        case 'h':
        default: show_help(); return 0;
        }
    }
    argc -= optind;
    argv += optind;

    if (file_exists(argv[0])) {
            if (mode == 0 && file_is_flif(argv[0])) {
              v_printf(2,"Input file is a FLIF file, adding implicit -d\n");
              mode = 1;
            }
            char *f = strrchr(argv[0],'/');
            char *ext = f ? strrchr(f,'.') : strrchr(argv[0],'.');
            if (mode == 0) {
                    if (ext && ( !strcasecmp(ext,".png") ||  !strcasecmp(ext,".pnm") ||  !strcasecmp(ext,".ppm")  ||  !strcasecmp(ext,".pgm") ||  !strcasecmp(ext,".pbm") ||  !strcasecmp(ext,".pam"))) {
                          // ok
                    } else {
                          fprintf(stderr,"Warning: expected \".png\" or \".pnm\" file name extension for input file, trying anyway...\n");
                    }
            } else {
                    if (ext && ( !strcasecmp(ext,".flif")  || ( !strcasecmp(ext,".flf") ))) {
                          // ok
                    } else {
                          fprintf(stderr,"Warning: expected file name extension \".flif\" for input file, trying anyway...\n");
                    }
            }
    } else if (argc>0) {
          fprintf(stderr,"Input file does not exist: %s\n",argv[0]);
          return 1;
    }


    v_printf(3,"  _____  __  (__) _____");
  v_printf(3,"\n (___  ||  | |  ||  ___)   ");v_printf(2,"FLIF 0.1 [2 October 2015]");
  v_printf(3,"\n  (__  ||  |_|__||  __)    Free Lossless Image Format");
  v_printf(3,"\n    (__||______) |__)      (c) 2010-2015 J.Sneyers & P.Wuille, GNU GPL v3+\n");
  v_printf(3,"\n");
  if (argc == 0) {
        //fprintf(stderr,"Input file missing.\n");
        if (verbosity == 1) show_help();
        return 1;
  }
  if (argc == 1) {
        fprintf(stderr,"Output file missing.\n");
        show_help();
        return 1;
  }

  if (mode == 0) {
        int nb_input_images = argc-1;
        while(argc>1) {
          Image image;
          v_printf(2,"\r");
          if (!image.load(argv[0])) {
            fprintf(stderr,"Could not read input file: %s\n", argv[0]);
            return 2;
          };
          images.push_back(image);
          if (image.rows() != images[0].rows() || image.cols() != images[0].cols() || image.numPlanes() != images[0].numPlanes()) {
            fprintf(stderr,"Dimensions of all input images should be the same!\n");
            fprintf(stderr,"  First image is %ux%u, %i channels.\n",images[0].cols(),images[0].rows(),images[0].numPlanes());
            fprintf(stderr,"  This image is %ux%u, %i channels: %s\n",image.cols(),image.rows(),image.numPlanes(),argv[0]);
            return 2;
          }
          argc--; argv++;
          if (nb_input_images>1) {v_printf(2,"    (%i/%i)         ",(int)images.size(),nb_input_images); v_printf(4,"\n");}
        }
        v_printf(2,"\n");
        bool flat=true;
        for (Image &image : images) if (image.uses_alpha()) flat=false;
        if (flat && images[0].numPlanes() == 4) {
              v_printf(2,"Alpha channel not actually used, dropping it.\n");
              for (Image &image : images) image.drop_alpha();
        }
        uint64_t nb_pixels = (uint64_t)images[0].rows() * images[0].cols();
        std::vector<std::string> desc;
        desc.push_back("YIQ");  // convert RGB(A) to YIQ(A)
        desc.push_back("BND");  // get the bounds of the color spaces
        if (palette_size > 0)
          desc.push_back("PLA");  // try palette (including alpha)
        if (palette_size > 0)
          desc.push_back("PLT");  // try palette (without alpha)
        if (acb == -1) {
          // not specified if ACB should be used
          if (nb_pixels > 10000) desc.push_back("ACB");  // try auto color buckets on large images
        } else if (acb) desc.push_back("ACB");  // try auto color buckets if forced
        if (method == 0) {
          // no method specified, pick one heuristically
          if (nb_pixels < 10000) method=1; // if the image is small, not much point in doing interlacing
          else method=2; // default method: interlacing
        }
        if (images.size() > 1) {
          desc.push_back("DUP");  // find duplicate frames
          desc.push_back("FRS");  // get the shapes of the frames
          if (lookback != 0) desc.push_back("FRA");  // make a "deep" alpha channel (negative values are transparent to some previous frame)
        }
        if (learn_repeats < 0) {
          // no number of repeats specified, pick a number heuristically
          learn_repeats = TREE_LEARN_REPEATS;
          if (nb_pixels < 5000) learn_repeats--;        // avoid large trees for small images
          if (learn_repeats < 0) learn_repeats=0;
        }
        encode(argv[0], images, desc, method, learn_repeats, acb, frame_delay, palette_size, lookback);
  } else {
        char *ext = strrchr(argv[1],'.');
        if (ext && ( !strcasecmp(ext,".png") ||  !strcasecmp(ext,".pnm") ||  !strcasecmp(ext,".ppm")  ||  !strcasecmp(ext,".pgm") ||  !strcasecmp(ext,".pbm") ||  !strcasecmp(ext,".pam"))) {
                 // ok
        } else {
           fprintf(stderr,"Error: expected \".png\", \".pnm\" or \".pam\" file name extension for output file\n");
           return 1;
        }
        if (!decode(argv[0], images, quality, scale)) return 3;
        if (scale>1)
          v_printf(3,"Downscaling output: %ux%u -> %ux%u\n",images[0].cols(),images[0].rows(),images[0].cols()/scale,images[0].rows()/scale);
        if (images.size() == 1) {
          if (!images[0].save(argv[1],scale)) return 2;
        } else {
          int counter=0;
          char filename[strlen(argv[1])+6];
          strcpy(filename,argv[1]);
          char *a_ext = strrchr(filename,'.');
          for (Image& image : images) {
             sprintf(a_ext,"-%03d%s",counter++,ext);
             if (!image.save(filename,scale)) return 2;
             v_printf(2,"    (%i/%i)         \r",counter,(int)images.size()); v_printf(4,"\n");
          }
        }
        v_printf(2,"\n");
  }
  for (Image &image : images) image.clear();
  return 0;
}
