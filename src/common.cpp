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
    return predict_and_calcProps_scanlines_plane<GeneralPlane,false>(properties,ranges,image,image.getPlane(p),p,r,c,min,max, fallback);
}


const int NB_PROPERTIES[] = {7,9,8,7,7};
const int NB_PROPERTIESA[] = {8,10,9,7,7};

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
    if (p==1 || p==2) propRanges.push_back(std::make_pair(ranges.min(0)-ranges.max(0),ranges.max(0)-ranges.min(0))); // luma prediction miss
    propRanges.push_back(std::make_pair(mind,maxd)); // neighbor A - neighbor B   (top-bottom or left-right)
    propRanges.push_back(std::make_pair(mind,maxd)); // previous pixel (left or top) prediction miss
    propRanges.push_back(std::make_pair(min,max));   // guess
    //if (p<1 || p>2) propRanges.push_back(std::make_pair(0,2));       // which predictor was it
    propRanges.push_back(std::make_pair(mind,maxd));  // left - topleft
    propRanges.push_back(std::make_pair(mind,maxd));  // topleft - top
    //propRanges.push_back(std::make_pair(mind,maxd));

    if (p != 2) {
      propRanges.push_back(std::make_pair(mind,maxd)); // toptop - top
      propRanges.push_back(std::make_pair(mind,maxd)); // leftleft - left
    }
}

// Actual prediction. Also sets properties. Property vector should already have the right size before calling this.
// This is a fall-back function which should be replaced by direct calls to the specific predict_and_calcProps_plane function
ColorVal predict_and_calcProps(Properties &properties, const ColorRanges *ranges, const Image &image, const int z, const int p, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max) ATTRIBUTE_HOT;
ColorVal predict_and_calcProps(Properties &properties, const ColorRanges *ranges, const Image &image, const int z, const int p, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max) {
    image.getPlane(0).prepare_zoomlevel(z);
    image.getPlane(p).prepare_zoomlevel(z);
    switch(p) {
      case 0:
        if (z%2==0) return predict_and_calcProps_plane<GeneralPlane,GeneralPlane,true,false,0,ColorRanges>(properties,ranges,image,image.getPlane(p),image.getPlane(0),z,r,c,min,max);
        else return predict_and_calcProps_plane<GeneralPlane,GeneralPlane,false,false,0,ColorRanges>(properties,ranges,image,image.getPlane(p),image.getPlane(0),z,r,c,min,max);
      case 1:
        if (z%2==0) return predict_and_calcProps_plane<GeneralPlane,GeneralPlane,true,false,1,ColorRanges>(properties,ranges,image,image.getPlane(p),image.getPlane(0),z,r,c,min,max);
        else return predict_and_calcProps_plane<GeneralPlane,GeneralPlane,false,false,1,ColorRanges>(properties,ranges,image,image.getPlane(p),image.getPlane(0),z,r,c,min,max);
      case 2:
        if (z%2==0) return predict_and_calcProps_plane<GeneralPlane,GeneralPlane,true,false,2,ColorRanges>(properties,ranges,image,image.getPlane(p),image.getPlane(0),z,r,c,min,max);
        else return predict_and_calcProps_plane<GeneralPlane,GeneralPlane,false,false,2,ColorRanges>(properties,ranges,image,image.getPlane(p),image.getPlane(0),z,r,c,min,max);
      case 3:
        if (z%2==0) return predict_and_calcProps_plane<GeneralPlane,GeneralPlane,true,false,3,ColorRanges>(properties,ranges,image,image.getPlane(p),image.getPlane(0),z,r,c,min,max);
        else return predict_and_calcProps_plane<GeneralPlane,GeneralPlane,false,false,3,ColorRanges>(properties,ranges,image,image.getPlane(p),image.getPlane(0),z,r,c,min,max);
      default:
        assert(p==4);
        if (z%2==0) return predict_and_calcProps_plane<GeneralPlane,GeneralPlane,true,false,4,ColorRanges>(properties,ranges,image,image.getPlane(p),image.getPlane(0),z,r,c,min,max);
        else return predict_and_calcProps_plane<GeneralPlane,GeneralPlane,false,false,4,ColorRanges>(properties,ranges,image,image.getPlane(p),image.getPlane(0),z,r,c,min,max);
    }
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
