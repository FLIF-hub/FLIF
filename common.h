#ifndef FLIF_COMMON_H
#define FLIF_COMMON_H

#include <string>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "maniac/rac.h"
#include "maniac/compound.h"
#include "maniac/util.h"

#include "image/color_range.h"

#include "flif_config.h"

extern std::vector<ColorVal> grey; // a pixel with values in the middle of the bounds
extern int64_t pixels_todo;
extern int64_t pixels_done;

#define MAX_TRANSFORM 8

extern const std::vector<std::string> transforms;

void increase_verbosity();
int get_verbosity();
void v_printf(const int v, const char *format, ...);

typedef SimpleBitChance                         FLIFBitChancePass1;

// faster:
//typedef SimpleBitChance                         FLIFBitChancePass2;
//typedef SimpleBitChance                         FLIFBitChanceParities;

// better compression:
typedef MultiscaleBitChance<6,SimpleBitChance>  FLIFBitChancePass2;
typedef MultiscaleBitChance<6,SimpleBitChance>  FLIFBitChanceParities;

typedef MultiscaleBitChance<6,SimpleBitChance>  FLIFBitChanceTree;

extern const int NB_PROPERTIES[];
extern const int NB_PROPERTIESA[];

extern const int NB_PROPERTIES_scanlines[];
extern const int NB_PROPERTIES_scanlinesA[];

void initPropRanges_scanlines(Ranges &propRanges, const ColorRanges &ranges, int p);

ColorVal predict_and_calcProps_scanlines(Properties &properties, const ColorRanges *ranges, const Image &image, const int p, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max);

void initPropRanges(Ranges &propRanges, const ColorRanges &ranges, int p);

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
ColorVal predict_and_calcProps(Properties &properties, const ColorRanges *ranges, const Image &image, const int z, const int p, const uint32_t r, const uint32_t c, ColorVal &min, ColorVal &max);

int plane_zoomlevels(const Image &image, const int beginZL, const int endZL);

std::pair<int, int> plane_zoomlevel(const Image &image, const int beginZL, const int endZL, int i);

#endif // __COMMMON_H__
