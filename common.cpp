#include "common.h"

std::vector<ColorVal> grey; // a pixel with values in the middle of the bounds

const std::vector<std::string> transforms = {"YIQ","BND","ACB","PLT","PLA","FRS","DUP","FRA","???"};

int64_t pixels_todo = 0;
int64_t pixels_done = 0;

const int NB_PROPERTIES_scanlines[] = {7,8,9,7};
const int NB_PROPERTIES_scanlinesA[] = {8,9,10,7};


static int verbosity = 1;
void increase_verbosity() {
    verbosity ++;
}

int get_verbosity() {
    return verbosity;
}

void v_printf(const int v, const char *format, ...) {
    if (verbosity < v) return;
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    fflush(stdout);
    va_end(args);
}

void initPropRanges_scanlines(Ranges &propRanges, const ColorRanges &ranges, int p) {
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


const int NB_PROPERTIES[] = {8,7,8,8};
const int NB_PROPERTIESA[] = {9,8,9,8};

void initPropRanges(Ranges &propRanges, const ColorRanges &ranges, int p) {
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

