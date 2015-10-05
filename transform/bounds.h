#ifndef FLIF_BOUNDS_H
#define FLIF_BOUNDS_H 1

#include <vector>

#include "transform.h"
#include "../maniac/symbol.h"

class ColorRangesBounds : public ColorRanges
{
protected:
    std::vector<std::pair<ColorVal, ColorVal> > bounds;
    const ColorRanges *ranges;
public:
    ColorRangesBounds(const std::vector<std::pair<ColorVal, ColorVal> > &boundsIn, const ColorRanges *rangesIn) : bounds(boundsIn), ranges(rangesIn) {}
    bool isStatic() const { return false; }
    int numPlanes() const { return bounds.size(); }
    ColorVal min(int p) const { assert(p<numPlanes()); return std::max(ranges->min(p), bounds[p].first); }
    ColorVal max(int p) const { assert(p<numPlanes()); return std::min(ranges->max(p), bounds[p].second); }
    void minmax(const int p, const prevPlanes &pp, ColorVal &min, ColorVal &max) const {
        assert(p<numPlanes());
        if (p==0) { min=bounds[p].first; max=bounds[p].second; return; } // optimization for special case
        ranges->minmax(p, pp, min, max);
        if (min < bounds[p].first) min=bounds[p].first;
        if (max > bounds[p].second) max=bounds[p].second;
    }
};


template <typename IO>
class TransformBounds : public Transform<IO> {
protected:
    std::vector<std::pair<ColorVal, ColorVal> > bounds;

    const ColorRanges *meta(Images& images, const ColorRanges *srcRanges) {
        if (srcRanges->isStatic()) {
            return new StaticColorRanges(bounds);
        } else {
            return new ColorRangesBounds(bounds, srcRanges);
        }
    }

    void load(const ColorRanges *srcRanges, RacIn<IO> &rac) {
        SimpleSymbolCoder<SimpleBitChance, RacIn<IO>, 24> coder(rac);
        bounds.clear();
        for (int p=0; p<srcRanges->numPlanes(); p++) {
//            ColorVal min = coder.read_int(0, srcRanges->max(p) - srcRanges->min(p)) + srcRanges->min(p);
//            ColorVal max = coder.read_int(0, srcRanges->max(p) - min) + min;
            ColorVal min = coder.read_int(srcRanges->min(p), srcRanges->max(p));
            ColorVal max = coder.read_int(min, srcRanges->max(p));
            bounds.push_back(std::make_pair(min,max));
            v_printf(5,"[%i:%i..%i]",p,min,max);
        }
    }

    void save(const ColorRanges *srcRanges, RacOut<IO> &rac) const {
        SimpleSymbolCoder<SimpleBitChance, RacOut<IO>, 24> coder(rac);
        for (int p=0; p<srcRanges->numPlanes(); p++) {
            ColorVal min = bounds[p].first;
            ColorVal max = bounds[p].second;
//            coder.write_int(0, srcRanges->max(p) - srcRanges->min(p), min - srcRanges->min(p));
//            coder.write_int(0, srcRanges->max(p) - min, max - min);
            coder.write_int(srcRanges->min(p), srcRanges->max(p), min);
            coder.write_int(min, srcRanges->max(p), max);
            v_printf(5,"[%i:%i..%i]",p,min,max);
        }
    }

    bool process(const ColorRanges *srcRanges, const Images &images) {
        bounds.clear();
        bool trivialbounds=true;
        for (int p=0; p<srcRanges->numPlanes(); p++) {
            ColorVal min = srcRanges->max(p);
            ColorVal max = srcRanges->min(p);
            for (const Image& image : images)
            for (uint32_t r=0; r<image.rows(); r++) {
                for (uint32_t c=0; c<image.cols(); c++) {
                    ColorVal v = image(p,r,c);
                    if (v < min) min = v;
                    if (v > max) max = v;
                    assert(v <= srcRanges->max(p));
                    assert(v >= srcRanges->min(p));
                }
            }
            bounds.push_back(std::make_pair(min,max));
            if (min > srcRanges->min(p)) trivialbounds=false;
            if (max < srcRanges->max(p)) trivialbounds=false;
        }
        return !trivialbounds;
    }
};

#endif
