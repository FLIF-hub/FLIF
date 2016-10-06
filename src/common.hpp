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

#include <memory>
#include <string>
#include <string.h>

#include "maniac/rac.hpp"
#include "maniac/compound.hpp"
#include "maniac/util.hpp"

#include "image/color_range.hpp"

#include "flif_config.h"

#include "io.hpp"


extern int64_t pixels_todo;
extern int64_t pixels_done;
extern int progressive_qual_target;
extern int progressive_qual_shown;


#define MAX_TRANSFORM 13
#define MAX_PREDICTOR 2

extern const std::vector<std::string> transforms;

typedef SimpleBitChance                         FLIFBitChancePass1;

// faster:
#ifdef FAST_BUT_WORSE_COMPRESSION
typedef SimpleBitChance                         FLIFBitChancePass2;
typedef SimpleBitChance                         FLIFBitChanceTree;
#else
// better compression:
typedef MultiscaleBitChance<6,SimpleBitChance>  FLIFBitChancePass2;
typedef MultiscaleBitChance<6,SimpleBitChance>  FLIFBitChanceTree;
#endif

extern const int NB_PROPERTIES[];
extern const int NB_PROPERTIESA[];

extern const int NB_PROPERTIES_scanlines[];
extern const int NB_PROPERTIES_scanlinesA[];

extern const int PLANE_ORDERING[];

void initPropRanges_scanlines(Ranges &propRanges, const ColorRanges &ranges, int p);

ColorVal predict_and_calcProps_scanlines(Properties &properties, const ColorRanges *ranges, const Image &image, const int p, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max, const ColorVal fallback);

void initPropRanges(Ranges &propRanges, const ColorRanges &ranges, int p);

template<typename I> I inline median3(I a, I b, I c) {
    if (a < b) {
        if (b < c) {
          return b;
        } else {
          return a < c ? c : a;
        }
    } else {
       if (a < c) {
          return a;
       } else {
          return b < c ? c : b;
       }
    }
}

template <typename plane_t, bool nobordercases>
ColorVal predict_and_calcProps_scanlines_plane(Properties &properties, const ColorRanges *ranges, const Image &image, const plane_t &plane, const int p, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max, const ColorVal fallback) {
    ColorVal guess;
    int which = 0;
    int index=0;
    if (p < 3) {
        for (int pp = 0; pp < p; pp++) {
            properties[index++] = image(pp,r,c);
        }
        if (image.numPlanes()>3) properties[index++] = image(3,r,c);
    }
    ColorVal left = (nobordercases || c>0 ? plane.get(r,c-1) : (r > 0 ? plane.get(r-1, c) : fallback));
    ColorVal top = (nobordercases || r>0 ? plane.get(r-1,c) : left);
    ColorVal topleft = (nobordercases || (r>0 && c>0) ? plane.get(r-1,c-1) : (r > 0 ? top : left));
    ColorVal gradientTL = left + top - topleft;
    guess = median3(gradientTL, left, top);
    ranges->snap(p,properties,min,max,guess);
    assert(min >= ranges->min(p));
    assert(max <= ranges->max(p));
    assert(guess >= min);
    assert(guess <= max);
    if (guess == gradientTL) which = 0;
    else if (guess == left) which = 1;
    else if (guess == top) which = 2;

    properties[index++] = guess;
    properties[index++] = which;

    if (nobordercases || (c > 0 && r > 0)) { properties[index++] = left - topleft; properties[index++] = topleft - top; }
    else   { properties[index++] = 0; properties[index++] = 0;  }

    if (nobordercases || (c+1 < image.cols() && r > 0)) properties[index++] = top - plane.get(r-1,c+1); // top - topright
    else   properties[index++] = 0;
    if (nobordercases || r > 1) properties[index++] = plane.get(r-2,c)-top;    // toptop - top
    else properties[index++] = 0;
    if (nobordercases || c > 1) properties[index++] = plane.get(r,c-2)-left;    // leftleft - left
    else properties[index++] = 0;
    return guess;
}

// Prediction used for interpolation / alpha=0 pixels. Does not have to be the same as the guess used for encoding/decoding.
template <typename plane_t>
inline ColorVal predictScanlines_plane(const plane_t &plane, uint32_t r, uint32_t c, ColorVal grey) {
    ColorVal left = (c>0 ? plane.get(r,c-1) : (r > 0 ? plane.get(r-1, c) : grey));
    ColorVal top = (r>0 ? plane.get(r-1,c) : left);
    ColorVal topleft = (r>0 && c>0 ? plane.get(r-1,c-1) : top);
    ColorVal gradientTL = left + top - topleft;
    return median3(gradientTL, left, top);
}

// Prediction used for interpolation / alpha=0 pixels. Does not have to be the same as the guess used for encoding/decoding.
template <typename plane_t>
inline ColorVal predict_plane_horizontal(const plane_t &plane, int z, int p, uint32_t r, uint32_t c, uint32_t rows, const int predictor) {
    if (p==4) return 0;
    assert(z%2 == 0); // filling horizontal lines
    ColorVal top = plane.get(z,r-1,c);
    ColorVal bottom = (r+1 < rows ? plane.get(z,r+1,c) : top ); // (c > 0 ? image(p, z, r, c - 1) : top));
    if (predictor == 0) {
      ColorVal avg = (top + bottom)>>1;
      return avg;
    } else if (predictor == 1) {
      ColorVal avg = (top + bottom)>>1;
      ColorVal left = (c>0 ? plane.get(z,r,c-1) : top);
      ColorVal topleft = (c>0 ? plane.get(z,r-1,c-1) : top);
      ColorVal bottomleft = (c>0 && r+1 < rows ? plane.get(z,r+1,c-1) : left);
      return median3(avg, (ColorVal)(left+top-topleft), (ColorVal)(left+bottom-bottomleft));
    } else { // if (predictor == 2) {
      ColorVal left = (c>0 ? plane.get(z,r,c-1) : top);
      return median3(top,bottom,left);
    }
}

template <typename plane_t>
inline ColorVal predict_plane_vertical(const plane_t &plane, int z, int p, uint32_t r, uint32_t c, uint32_t cols, const int predictor) {
    if (p==4) return 0;
    assert(z%2 == 1); // filling vertical lines
    ColorVal left = plane.get(z,r,c-1);
    ColorVal right = (c+1 < cols ? plane.get(z,r,c+1) : left ); //(r > 0 ? image(p, z, r-1, c) : left));
    if (predictor == 0) {
      ColorVal avg = (left + right)>>1;
      return avg;
    } else if (predictor == 1) {
      ColorVal avg = (left + right)>>1;
      ColorVal top = (r>0 ? plane.get(z,r-1,c) : left);
      ColorVal topleft = (r>0 ? plane.get(z,r-1,c-1) : left);
      ColorVal topright = (r>0 && c+1 < cols ? plane.get(z,r-1,c+1) : top);
      return median3(avg, (ColorVal)(left+top-topleft), (ColorVal)(right+top-topright));
    } else { // if (predictor == 2) {
      ColorVal top = (r>0 ? plane.get(z,r-1,c) : left);
      return median3(top,left,right);
    }
}

// Prediction used for interpolation / alpha=0 pixels. Does not have to be the same as the guess used for encoding/decoding.
inline ColorVal predictScanlines(const Image &image, int p, uint32_t r, uint32_t c, ColorVal grey) {
    return predictScanlines_plane(image.getPlane(p),r,c,grey);
}

// Prediction used for interpolation / alpha=0 pixels. Does not have to be the same as the guess used for encoding/decoding.
inline ColorVal predict(const Image &image, int z, int p, uint32_t r, uint32_t c, const int predictor) {
    if (p==4) return 0;
    ColorVal prediction;
    if (z%2 == 0) { // filling horizontal lines
      prediction = predict_plane_horizontal(image.getPlane(p),z,p,r,c,image.rows(z),predictor);
    } else { // filling vertical lines
      prediction = predict_plane_vertical(image.getPlane(p),z,p,r,c,image.cols(z),predictor);
    }

    // accurate snap-to-ranges: too expensive?
/*
    if (p == 1 || p == 2) {
      prevPlanes pp(p);
      ColorVal min, max;
      for (int prev=0; prev<p; prev++) pp[prev]=image(prev,z,r,c);
      ranges->snap(p,pp,min,max,prediction);
    }
*/
    return prediction;

}

#define PIXEL(z,r,c) plane.get_fast(r,c)
#define PIXELY(z,r,c) planeY.get_fast(r,c)


// Actual prediction. Also sets properties. Property vector should already have the right size before calling this.
template <typename plane_t, typename plane_tY, bool horizontal, bool nobordercases, int p, typename ranges_t>
ColorVal predict_and_calcProps_plane(Properties &properties, const ranges_t *ranges, const Image &image, const plane_t &plane, const plane_tY &planeY, const int z, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max, const int predictor) ATTRIBUTE_HOT;
template <typename plane_t, typename plane_tY, bool horizontal, bool nobordercases, int p, typename ranges_t>
ColorVal predict_and_calcProps_plane(Properties &properties, const ranges_t *ranges, const Image &image, const plane_t &plane, const plane_tY &planeY, const int z, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max, const int predictor) {
    ColorVal guess;
    //int which = 0;
    int index = 0;

    if (p < 3) {
        if (p>0) properties[index++] = PIXELY(z,r,c);
        if (p>1) properties[index++] = image(1,z,r,c);
        if (image.numPlanes()>3) properties[index++] = image(3,z,r,c);
    }
    ColorVal left;
    ColorVal top;
    ColorVal topleft;

    const bool bottomPresent = r+1 < image.rows(z);
    const bool rightPresent = c+1 < image.cols(z);
    ColorVal topright;
    ColorVal bottomleft;

    if (horizontal) { // filling horizontal lines
        top = PIXEL(z,r-1,c);
        left = (nobordercases || c>0 ? PIXEL(z,r,c-1) : top);
        topleft = (nobordercases || c>0 ? PIXEL(z,r-1,c-1) : top);
        topright = (nobordercases || (rightPresent) ? PIXEL(z,r-1,c+1) : top);
        bottomleft = (nobordercases || (bottomPresent && c>0) ? PIXEL(z,r+1,c-1) : left);
        const ColorVal bottom = (nobordercases || bottomPresent ? PIXEL(z,r+1,c) : left);
        const ColorVal avg = (top + bottom)>>1;
        const ColorVal topleftgradient = left+top-topleft;
        const ColorVal median = median3(avg, topleftgradient, (ColorVal)(left+bottom-bottomleft));
        int which = 2;
        if (median == avg) which = 0;
        else if (median == topleftgradient) which = 1;
        properties[index++]=which;
        if (p == 1 || p == 2) {
          properties[index++] = PIXELY(z,r,c) - ((PIXELY(z,r-1,c)+PIXELY(z,(nobordercases || bottomPresent ? r+1 : r-1),c))>>1);
        }
        if (predictor == 0) guess = avg;
        else if (predictor == 1)
            guess = median;
        else //if (predictor == 2)
            guess = median3(top,bottom,left);
        ranges->snap(p,properties,min,max,guess);
        properties[index++] = top-bottom;
        properties[index++]=top-((topleft+topright)>>1);
        properties[index++]=left-((bottomleft+topleft)>>1);
        const ColorVal bottomright = (nobordercases || (rightPresent && bottomPresent) ? PIXEL(z,r+1,c+1) : bottom);
        properties[index++]=bottom-((bottomleft+bottomright)>>1);
    } else { // filling vertical lines
        left = PIXEL(z,r,c-1);
        top = (nobordercases || r>0 ? PIXEL(z,r-1,c) : left);
        topleft = (nobordercases || r>0 ? PIXEL(z,r-1,c-1) : left);
        topright = (nobordercases || (r>0 && rightPresent) ? PIXEL(z,r-1,c+1) : top);
        bottomleft = (nobordercases || (bottomPresent) ? PIXEL(z,r+1,c-1) : left);
        const ColorVal right = (nobordercases || rightPresent ? PIXEL(z,r,c+1) : top);
        const ColorVal avg = (left + right)>>1;
        const ColorVal topleftgradient = left+top-topleft;
        const ColorVal median = median3(avg, topleftgradient, (ColorVal)(right+top-topright));
        int which = 2;
        if (median == avg) which = 0;
        else if (median == topleftgradient) which = 1;
        properties[index++]=which;
        if (p == 1 || p == 2) {
          properties[index++] = PIXELY(z,r,c) - ((PIXELY(z,r,c-1)+PIXELY(z,r,(nobordercases || rightPresent ? c+1 : c-1)))>>1);
        }
        if (predictor == 0) guess = avg;
        else if (predictor == 1)
            guess = median;
        else //if (predictor == 2)
            guess = median3(top,left,right);
        ranges->snap(p,properties,min,max,guess);
        properties[index++] = left-right;
        properties[index++]=left-((bottomleft+topleft)>>1);
        properties[index++]=top-((topleft+topright)>>1);
        const ColorVal bottomright = (nobordercases || (rightPresent && bottomPresent) ? PIXEL(z,r+1,c+1) : right);
        properties[index++]=right-((bottomright+topright)>>1);
    }
    properties[index++]=guess;
//    if (p < 1 || p > 2) properties[index++]=which;


//    properties[index++]=left - topleft;
//    properties[index++]=topleft - top;

//    if (p == 0 || p > 2) {
//        if (nobordercases || (c+1 < image.cols(z) && r > 0)) properties[index++]=top - topright;
//        else properties[index++]=0;
//    }
    if (p != 2) {
        if (nobordercases || r > 1) properties[index++]=PIXEL(z,r-2,c)-top;    // toptop - top
        else properties[index++]=0;
        if (nobordercases || c > 1) properties[index++]=PIXEL(z,r,c-2)-left;    // leftleft - left
        else properties[index++]=0;
    }
    return guess;
}

ColorVal predict_and_calcProps(Properties &properties, const ColorRanges *ranges, const Image &image, const int z, const int p, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max, const int predictor);

int plane_zoomlevels(const Image &image, const int beginZL, const int endZL);

std::pair<int, int> plane_zoomlevel(const Image &image, const int beginZL, const int endZL, int i, const ColorRanges *ranges);

inline std::vector<ColorVal> computeGreys(const ColorRanges *ranges) {
    std::vector<ColorVal> greys; // a pixel with values in the middle of the bounds
    for (int p = 0; p < ranges->numPlanes(); p++) greys.push_back((ranges->min(p)+ranges->max(p))/2);
    return greys;
}
