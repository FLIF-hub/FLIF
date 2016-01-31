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

#include "precompile.h"

#include "common.hpp"

const std::vector<std::string> transforms = {"Channel_Compact", "YCoCg", "Bounds",
                                             "Palette_Alpha", "Palette", "?? Palette_Chroma ??", "Color_Buckets",
                                             "Duplicate_Frame", "Frame_Shape", "Frame_Lookback",
                                             "?? YCbCr ??", "?? DCT ??", "?? DWT ??", "?? Quantization ??",
                                             "?? Reserved ??", "?? Other ??" };
// Plenty of room for future extensions: transform "Other" can be used to encode identifiers of arbitrary many other transforms

int64_t pixels_todo = 0;
int64_t pixels_done = 0;
int progressive_qual_target = 0;
int progressive_qual_shown = -1;


const int PLANE_ORDERING[] = {4,3,0,1,2}; // FRA, A, Y, I, Q

const int NB_PROPERTIES_scanlines[] = {7,8,9,7,7};
const int NB_PROPERTIES_scanlinesA[] = {8,9,10,7,7};


void initPropRanges_scanlines(Ranges &propRanges, const ColorRanges &ranges, int p) {
    propRanges.clear();
    int min = ranges.min(p);
    int max = ranges.max(p);
    int mind = min - max, maxd = max - min;

    if (p < 3) {
      for (int pp = 0; pp < p; pp++) {
        propRanges.push_back(std::make_pair(ranges.min(pp), ranges.max(pp)));  // pixels on previous planes
      }
      if (ranges.numPlanes()>3) propRanges.push_back(std::make_pair(ranges.min(3), ranges.max(3)));  // pixel on alpha plane
    }
    propRanges.push_back(std::make_pair(min,max));   // guess (median of 3)
    propRanges.push_back(std::make_pair(0,2));       // which predictor was it
    propRanges.push_back(std::make_pair(mind,maxd));
    propRanges.push_back(std::make_pair(mind,maxd));
    propRanges.push_back(std::make_pair(mind,maxd));
    propRanges.push_back(std::make_pair(mind,maxd));
    propRanges.push_back(std::make_pair(mind,maxd));
}

ColorVal predict_and_calcProps_scanlines(Properties &properties, const ColorRanges *ranges, const Image &image, const int p, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max, const ColorVal fallback) {
    ColorVal guess;
    int which = 0;
    int index=0;
    if (p < 3) {
      for (int pp = 0; pp < p; pp++) {
        properties[index++] = image(pp,r,c);
      }
      if (image.numPlanes()>3) properties[index++] = image(3,r,c);
    }
    ColorVal left = (c>0 ? image(p,r,c-1) : (r > 0 ? image(p, r-1, c) : fallback));
    ColorVal top = (r>0 ? image(p,r-1,c) : left);
    ColorVal topleft = (r>0 && c>0 ? image(p,r-1,c-1) : (r > 0 ? top : left));
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

    if (c+1 < image.cols() && r > 0) properties[index++] = top - image(p,r-1,c+1); // top - topright
                 else   properties[index++] = 0;
    if (r > 1) properties[index++] = image(p,r-2,c)-top;    // toptop - top
         else properties[index++] = 0;
    if (c > 1) properties[index++] = image(p,r,c-2)-left;    // leftleft - left
         else properties[index++] = 0;
    return guess;
}


const int NB_PROPERTIES[] = {8,9,8,8,8};
const int NB_PROPERTIESA[] = {9,10,9,8,8};

void initPropRanges(Ranges &propRanges, const ColorRanges &ranges, int p) {
    propRanges.clear();
    int min = ranges.min(p);
    int max = ranges.max(p);
    int mind = min - max, maxd = max - min;
    if (p < 3) {       // alpha channel first
      for (int pp = 0; pp < p; pp++) {
        propRanges.push_back(std::make_pair(ranges.min(pp), ranges.max(pp)));  // pixels on previous planes
      }
      if (ranges.numPlanes()>3) propRanges.push_back(std::make_pair(ranges.min(3), ranges.max(3)));  // pixel on alpha plane
    }
    propRanges.push_back(std::make_pair(mind,maxd)); // neighbor A - neighbor B   (top-bottom or left-right)
    propRanges.push_back(std::make_pair(min,max));   // guess (median of 3)
    propRanges.push_back(std::make_pair(0,2));       // which predictor was it
    propRanges.push_back(std::make_pair(mind,maxd));
    propRanges.push_back(std::make_pair(mind,maxd));
    propRanges.push_back(std::make_pair(mind,maxd));

    if (p < 2 || p >= 3) {
      propRanges.push_back(std::make_pair(mind,maxd));
      propRanges.push_back(std::make_pair(mind,maxd));
    }
}

// Actual prediction. Also sets properties. Property vector should already have the right size before calling this.
ColorVal predict_and_calcProps(Properties &properties, const ColorRanges *ranges, const Image &image, const int z, const int p, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max) ATTRIBUTE_HOT;
ColorVal predict_and_calcProps(Properties &properties, const ColorRanges *ranges, const Image &image, const int z, const int p, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max) {
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
    if (z%2 == 0) { // filling horizontal lines
      top = image(p,z,r-1,c);
      left = (c>0 ? image(p,z,r,c-1) : top);
      topleft = (c>0 ? image(p,z,r-1,c-1) : top);
      topright = (c+1 < image.cols(z) ? image(p,z,r-1,c+1) : top);
      const ColorVal gradientTL = left + top - topleft;
      const bool bottomPresent = r+1 < image.rows(z);
      const ColorVal bottom = (bottomPresent ? image(p,z,r+1,c) : left);
      const ColorVal bottomleft = (bottomPresent && c>0 ? image(p,z,r+1,c-1) : bottom);
      const ColorVal gradientBL = left + bottom - bottomleft;
      const ColorVal avg = (top + bottom)>>1;
      guess = median3(gradientTL, gradientBL, avg);
      ranges->snap(p,properties,min,max,guess);
      if (guess == avg) which = 0;
      else if (guess == gradientTL) which = 1;
      else if (guess == gradientBL) which = 2;
      properties[index++] = top-bottom;
    } else { // filling vertical lines
      left = image(p,z,r,c-1);
      top = (r>0 ? image(p,z,r-1,c) : left);
      topleft = (r>0 ? image(p,z,r-1,c-1) : left);
      const bool rightPresent = c+1 < image.cols(z);
      const ColorVal right = (rightPresent ? image(p,z,r,c+1) : top);
      topright = (r>0 && rightPresent ? image(p,z,r-1,c+1) : right);
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

    if (c > 0 && r > 0) { properties[index++]=left - topleft; properties[index++]=topleft - top; }
                 else   { properties[index++]=0; properties[index++]=0; }

    if (c+1 < image.cols(z) && r > 0) properties[index++]=top - topright;
                 else   properties[index++]=0;

    if (p < 2 || p >= 3) {
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
    // assumption: plane 0 is Y, plane 1 is I, plane 2 is Q, plane 3 is perhaps alpha, plane 4 are frame lookbacks (FRA transform, animation only)
    const int max_behind[] = {0, 2, 4, 0, 0};
    int np = image.numPlanes();
    if (np>5) {
      // too many planes, do something simple
      int p = i % image.numPlanes();
      int zl = beginZL - (i / image.numPlanes());
      return std::pair<int, int>(p,zl);
    }
    std::vector<int> czl(np);
    for (int &pzl : czl) pzl = beginZL+1;
    int highest_priority_plane = 0;
    if (np >= 4) highest_priority_plane = 3; // alpha first
    if (np >= 5) highest_priority_plane = 4; // lookbacks first
    int nextp = highest_priority_plane;
    while (i >= 0) {
      czl[nextp]--;
      i--;
      if (i<0) break;
      nextp=highest_priority_plane;
      for (int p=0; p<np; p++) {
        if (czl[p] > czl[highest_priority_plane] + max_behind[p]) {
          nextp = p; //break;
        }
      }
      // ensure that nextp is not at the most detailed zoomlevel yet
      while (czl[nextp] <= endZL) nextp = (nextp+1)%np;
    }
    int p = nextp;
    int zl = czl[p];

    return std::pair<int, int>(p,zl);
}
