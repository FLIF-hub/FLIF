#ifndef FLIF_YIQ_H
#define FLIF_YIQ_H 1

#include "../image/image.h"
#include "../image/color_range.h"
#include "transform.h"
#include <algorithm>

#define clip(x,l,u)   if (x < l) x=l; if (x > u) x=u

ColorVal static inline get_min_y(int par) {
    return 0;
}

ColorVal static inline get_max_y(int par) {
    return par*4-1;
}

ColorVal static inline get_min_i(int par, ColorVal y) {
    assert(y >= get_min_y(par));
    assert(y <= get_max_y(par));

    if (y<par-1) {
      return 4*par-4-4*y;
    } else if (y>=3*par) {
      return 3+4*(y-3*par);
    } else {
      return 0;
    }
}

ColorVal static inline get_max_i(int par, ColorVal y) {
    assert(y >= get_min_y(par));
    assert(y <= get_max_y(par));

    if (y<par-1) {
      return 4*par+2+4*y;
    } else if (y>=3*par) {
      return 8*par-5-4*(y-3*par);
    } else {
      return 8*par-2;
    }
}

ColorVal static inline get_min_q(int par, ColorVal y, ColorVal i) {
    assert(y >= get_min_y(par));
    assert(y <= get_max_y(par));
    if (i < get_min_i(par,y)) return 8*par; //invalid value
    if (i > get_max_i(par,y)) return 8*par; //invalid value
    assert(i >= get_min_i(par,y));
    assert(i <= get_max_i(par,y));

    if (y<par-1) {
      return 4*par-2-2*y+(abs(i-4*par+1)/2)*2;
    } else if (y>=3*par) {
      return 4*par-1-2*(4*par-1-y);
    } else {
      return std::max(1+(y-2*par)*2, 2*par-(y-par+1)*2+(abs(i-4*par+1)/2)*2);
    }
}

ColorVal static inline get_max_q(int par, ColorVal y, ColorVal i) {
    assert(y >= get_min_y(par));
    assert(y <= get_max_y(par));
    if (i < get_min_i(par,y)) return -1; //invalid value
    if (i > get_max_i(par,y)) return -1; //invalid value
    assert(i >= get_min_i(par,y));
    assert(i <= get_max_i(par,y));

    if (y<par-1) {
      return 4*par+2*y;
    } else if (y>=3*par) {
      return 4*par-1+2*(4*par-1-y)-((1+abs(i-4*par+1))/2)*2;
    } else {
      return (std::min)(6*par-2+(y-par+1)*2, 6*par-1+(3*par-1-y)*2-((1+abs(i-4*par+1))/2)*2);
    }
}


class ColorRangesYIQ : public ColorRanges
{
protected:
//    const int par=64; // range: [0..4*par-1]
    int par;
    const ColorRanges *ranges;
public:
    ColorRangesYIQ(int parIn, const ColorRanges *rangesIn)
            : par(parIn), ranges(rangesIn) {
    //        if (parIn != par) printf("OOPS: using YIQ transform on something other than rgb888 ?\n");
    }
    bool isStatic() const { return false; }
    int numPlanes() const { return ranges->numPlanes(); }

    ColorVal min(int p) const { if (p<3) return 0; else return ranges->min(p); }
    ColorVal max(int p) const { switch(p) {
                                        case 0: return 4*par-1;
                                        case 1: return 8*par-2;
                                        case 2: return 8*par-2;
                                        default: return ranges->max(p);
                                         };
                              }
    void minmax(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv) const {
         if (p==1) { minv=get_min_i(par, pp[0]); maxv=get_max_i(par, pp[0]); return; }
         else if (p==2) { minv=get_min_q(par, pp[0], pp[1]); maxv=get_max_q(par, pp[0], pp[1]); return; }
         else if (p==0) { minv=0; maxv=get_max_y(par); return;}
         else ranges->minmax(p,pp,minv,maxv);
    }
};


template <typename IO>
class TransformYIQ : public Transform<IO> {
protected:
    int par;

public:
    bool virtual init(const ColorRanges *srcRanges) {
        if (srcRanges->numPlanes() < 3) return false;
        if (srcRanges->min(0) < 0 || srcRanges->min(1) < 0 || srcRanges->min(2) < 0) return false;
        int max = std::max(std::max(srcRanges->max(0), srcRanges->max(1)), srcRanges->max(2));
        par = max/4+1;
        return true;
    }

    const ColorRanges *meta(Images& images, const ColorRanges *srcRanges) {
        return new ColorRangesYIQ(par, srcRanges);
    }

    void data(Images& images) const {
//        printf("TransformYIQ::data: par=%i\n", par);
        for (Image& image : images)
        for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                int R=image(0,r,c), G=image(1,r,c), B=image(2,r,c);

                int Y = ((R + B) / 2 + G) / 2;
                int I = R - B + par*4 - 1;
                int Q = (R + B) / 2 - G + par*4 - 1;

                image.set(0,r,c, Y);
                image.set(1,r,c, I);
                image.set(2,r,c, Q);
            }
        }
    }

    void invData(Images& images) const {
        for (Image& image : images)
        for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                int Y=image(0,r,c), I=image(1,r,c), Q=image(2,r,c);

                int R = Y + (Q + 2) / 2 + (I + 2) / 2 - 4*par;
                int G = Y - (Q + 1) / 2 + 2*par;
                int B = Y + (Q + 2) / 2 - (I + 1) / 2;

                // clipping only needed in case of lossy/partial decoding
                clip(R, 0, par*4-1);
                clip(G, 0, par*4-1);
                clip(B, 0, par*4-1);
                image.set(0,r,c, R);
                image.set(1,r,c, G);
                image.set(2,r,c, B);
            }
        }
    }
};


#endif
