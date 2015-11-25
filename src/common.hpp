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

extern std::vector<ColorVal> grey; // a pixel with values in the middle of the bounds
extern int64_t pixels_todo;
extern int64_t pixels_done;
extern int progressive_qual_target;


#define MAX_TRANSFORM 9

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

ColorVal predict_and_calcProps_scanlines(Properties &properties, const ColorRanges *ranges, const Image &image, const int p, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max);

void initPropRanges(Ranges &propRanges, const ColorRanges &ranges, int p);

// Prediction used for interpolation / alpha=0 pixels. Does not have to be the same as the guess used for encoding/decoding.
inline ColorVal predict(const Image &image, int p, uint32_t r, uint32_t c) {
    ColorVal left = (c>0 ? image(p,r,c-1) : grey[p]);;
    ColorVal top = (r>0 ? image(p,r-1,c) : grey[p]);
    ColorVal topleft = (r>0 && c>0 ? image(p,r-1,c-1) : grey[p]);
    ColorVal gradientTL = left + top - topleft;
    return maniac::util::median3(gradientTL, left, top);
}

// Prediction used for interpolation / alpha=0 pixels. Does not have to be the same as the guess used for encoding/decoding.
inline ColorVal predict(const Image &image, int z, int p, uint32_t r, uint32_t c) {
    if (p==4) return 0;
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
ColorVal predict_and_calcProps(Properties &properties, const ColorRanges *ranges, const Image &image, const int z, const int p, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max);

int plane_zoomlevels(const Image &image, const int beginZL, const int endZL);

std::pair<int, int> plane_zoomlevel(const Image &image, const int beginZL, const int endZL, int i);
