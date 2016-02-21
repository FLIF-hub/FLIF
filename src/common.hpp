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

enum class Optional : uint8_t {
  undefined = 0
};

enum class flifEncoding : uint8_t {
  nonInterlaced = 1,
  interlaced = 2
};

extern int64_t pixels_todo;
extern int64_t pixels_done;
extern int progressive_qual_target;
extern int progressive_qual_shown;


#define MAX_TRANSFORM 15

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

template <typename plane_t>
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
    ColorVal left = (c>0 ? plane.get(r,c-1) : (r > 0 ? plane.get(r-1, c) : fallback));
    ColorVal top = (r>0 ? plane.get(r-1,c) : left);
    ColorVal topleft = (r>0 && c>0 ? plane.get(r-1,c-1) : (r > 0 ? top : left));
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

    if (c > 0 && r > 0) { properties[index++] = left - topleft; properties[index++] = topleft - top; }
    else   { properties[index++] = 0; properties[index++] = 0;  }

    if (c+1 < image.cols() && r > 0) properties[index++] = top - plane.get(r-1,c+1); // top - topright
    else   properties[index++] = 0;
    if (r > 1) properties[index++] = plane.get(r-2,c)-top;    // toptop - top
    else properties[index++] = 0;
    if (c > 1) properties[index++] = plane.get(r,c-2)-left;    // leftleft - left
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
inline ColorVal predict_plane_horizontal(const plane_t &plane, int z, int p, uint32_t r, uint32_t c, uint32_t rows) {
    if (p==4) return 0;
    assert(z%2 == 0); // filling horizontal lines
    ColorVal top = plane.get(z,r-1,c);
    ColorVal bottom = (r+1 < rows ? plane.get(z,r+1,c) : top ); // (c > 0 ? image(p, z, r, c - 1) : top));
    ColorVal avg = (top + bottom)>>1;
    return avg;
}

template <typename plane_t>
inline ColorVal predict_plane_vertical(const plane_t &plane, int z, int p, uint32_t r, uint32_t c, uint32_t cols) {
    if (p==4) return 0;
    assert(z%2 == 1); // filling vertical lines
    ColorVal left = plane.get(z,r,c-1);
    ColorVal right = (c+1 < cols ? plane.get(z,r,c+1) : left ); //(r > 0 ? image(p, z, r-1, c) : left));
    ColorVal avg = (left + right)>>1;
    return avg;
}

// Prediction used for interpolation / alpha=0 pixels. Does not have to be the same as the guess used for encoding/decoding.
inline ColorVal predictScanlines(const Image &image, int p, uint32_t r, uint32_t c, ColorVal grey) {
    return predictScanlines_plane(image.getPlane(p),r,c,grey);
}

// Prediction used for interpolation / alpha=0 pixels. Does not have to be the same as the guess used for encoding/decoding.
inline ColorVal predict(const Image &image, int z, int p, uint32_t r, uint32_t c) {
    if (p==4) return 0;
    if (z%2 == 0) { // filling horizontal lines
      return predict_plane_horizontal(image.getPlane(p),z,p,r,c,image.rows(z));
    } else { // filling vertical lines
      return predict_plane_vertical(image.getPlane(p),z,p,r,c,image.cols(z));
    }
}

// Actual prediction. Also sets properties. Property vector should already have the right size before calling this.
template <typename plane_t, bool horizontal, bool nobordercases>
ColorVal predict_and_calcProps_plane(Properties &properties, const ColorRanges *ranges, const Image &image, const plane_t &plane, const int z, const int p, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max) ATTRIBUTE_HOT;
template <typename plane_t, bool horizontal, bool nobordercases>
ColorVal predict_and_calcProps_plane(Properties &properties, const ColorRanges *ranges, const Image &image, const plane_t &plane, const int z, const int p, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max) {
    ColorVal guess;
    int which = 0;
    int index = 0;

    if (p < 3) {
        for (int pp = 0; pp < p; pp++) {
            properties[index++] = image(pp,z,r,c);
        }
        if (image.numPlanes()>3) properties[index++] = image(3,z,r,c);
    }
    ColorVal left;
    ColorVal top;
    ColorVal topleft;
    ColorVal topright;
    if (horizontal) { // filling horizontal lines
        top = plane.get(z,r-1,c);
        left = (nobordercases || c>0 ? plane.get(z,r,c-1) : top);
        topleft = (nobordercases || c>0 ? plane.get(z,r-1,c-1) : top);
        topright = (nobordercases || c+1 < image.cols(z) ? plane.get(z,r-1,c+1) : top);
        const ColorVal gradientTL = left + top - topleft;
        const bool bottomPresent = r+1 < image.rows(z);
        const ColorVal bottom = (nobordercases || bottomPresent ? plane.get(z,r+1,c) : left);
        const ColorVal bottomleft = (nobordercases || (bottomPresent && c>0) ? plane.get(z,r+1,c-1) : bottom);
        const ColorVal gradientBL = left + bottom - bottomleft;
        const ColorVal avg = (top + bottom)>>1;
        guess = median3(gradientTL, gradientBL, avg);
        ranges->snap(p,properties,min,max,guess);
        if (guess == avg) which = 0;
        else if (guess == gradientTL) which = 1;
        else if (guess == gradientBL) which = 2;
        properties[index++] = top-bottom;
    } else { // filling vertical lines
        left = plane.get(z,r,c-1);
        top = (nobordercases || r>0 ? plane.get(z,r-1,c) : left);
        topleft = (nobordercases || r>0 ? plane.get(z,r-1,c-1) : left);
        const bool rightPresent = c+1 < image.cols(z);
        const ColorVal right = (nobordercases || rightPresent ? plane.get(z,r,c+1) : top);
        topright = (nobordercases || (r>0 && rightPresent) ? plane.get(z,r-1,c+1) : right);
        const ColorVal gradientTL = left + top - topleft;
        const ColorVal gradientTR = right + top - topright;
        ColorVal avg = (left + right)>>1;
        guess = median3(gradientTL, gradientTR, avg);
        ranges->snap(p,properties,min,max,guess);
        if (guess == avg) which = 0;
        else if (guess == gradientTL) which = 1;
        else if (guess == gradientTR) which = 2;
        properties[index++] = left-right;
    }
    properties[index++]=guess;
    properties[index++]=which;

    if (nobordercases || (c > 0 && r > 0)) { properties[index++]=left - topleft; properties[index++]=topleft - top; }
    else   { properties[index++]=0; properties[index++]=0; }

    if (nobordercases || (c+1 < image.cols(z) && r > 0)) properties[index++]=top - topright;
    else   properties[index++]=0;

    if (p != 2) {
        if (nobordercases || r > 1) properties[index++]=plane.get(z,r-2,c)-top;    // toptop - top
        else properties[index++]=0;
        if (nobordercases || c > 1) properties[index++]=plane.get(z,r,c-2)-left;    // leftleft - left
        else properties[index++]=0;
    }
    return guess;
}

ColorVal predict_and_calcProps(Properties &properties, const ColorRanges *ranges, const Image &image, const int z, const int p, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max);

int plane_zoomlevels(const Image &image, const int beginZL, const int endZL);

std::pair<int, int> plane_zoomlevel(const Image &image, const int beginZL, const int endZL, int i);

inline std::vector<ColorVal> computeGreys(const ColorRanges *ranges) {
    std::vector<ColorVal> greys; // a pixel with values in the middle of the bounds
    for (int p = 0; p < ranges->numPlanes(); p++) greys.push_back((ranges->min(p)+ranges->max(p))/2);
    return greys;
}
