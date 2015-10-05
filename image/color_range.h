#ifndef FLIF_COLOR_RANGE_H
#define FLIF_COLOR_RANGE_H 1

#include <vector>

#include "image.h"

typedef std::vector<ColorVal> prevPlanes;


class ColorRanges
{
public:
    virtual ~ColorRanges() {};
    virtual int numPlanes() const =0;
    virtual ColorVal min(int p) const =0;
    virtual ColorVal max(int p) const =0;
    virtual void minmax(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv) const { minv=min(p); maxv=max(p); }
    virtual void snap(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv, ColorVal &v) const {
        minmax(p,pp,minv,maxv);
        if(v>maxv) v=maxv;
        if(v<minv) v=minv;
    }
    virtual bool isStatic() const { return true; }
};

typedef std::vector<std::pair<ColorVal, ColorVal> > StaticColorRangeList;

class StaticColorRanges : public ColorRanges
{
protected:
    StaticColorRangeList ranges;

public:
    StaticColorRanges(StaticColorRangeList ranges) { this->ranges = ranges; }
    int numPlanes() const { return ranges.size(); }
    ColorVal min(int p) const { assert(p<numPlanes()); return ranges[p].first; }
    ColorVal max(int p) const { assert(p<numPlanes()); return ranges[p].second; }
};

const ColorRanges *getRanges(const Image &image);

class DupColorRanges : public ColorRanges {
protected:
    const ColorRanges *ranges;
public:
    DupColorRanges(const ColorRanges *rangesIn) : ranges(rangesIn) {}

    int numPlanes() const { return ranges->numPlanes(); }
    ColorVal min(int p) const { return ranges->min(p); }
    ColorVal max(int p) const { return ranges->max(p); }
    void minmax(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv) const { ranges->minmax(p,pp,minv,maxv); }
    bool isStatic() const { return ranges->isStatic(); }
};

const ColorRanges *dupRanges(const ColorRanges *ranges);

#endif
