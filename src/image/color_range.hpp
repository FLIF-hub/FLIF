#pragma once

#include <vector>

#include "image.hpp"

typedef std::vector<ColorVal> prevPlanes;


class ColorRanges
{
public:
    virtual ~ColorRanges() {};
    virtual int numPlanes() const =0;
    virtual ColorVal min(int p) const =0;
    virtual ColorVal max(int p) const =0;
    virtual void minmax(const int p, const prevPlanes &, ColorVal &minv, ColorVal &maxv) const { minv=min(p); maxv=max(p); }
    virtual void snap(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv, ColorVal &v) const {
        minmax(p,pp,minv,maxv);
        if (minv > maxv) { //e_printf("Corruption detected!\n");
            // this should only happen on malicious/corrupt input files, or while adding loss
           maxv=minv;
        }
        assert(minv <= maxv);
        if(v>maxv) v=maxv;
        if(v<minv) v=minv;
        assert(v <= maxv);
        assert(v >= minv);
    }
    virtual bool isStatic() const { return true; }
    virtual const ColorRanges* previous() const { return NULL; }
};

typedef std::vector<std::pair<ColorVal, ColorVal> > StaticColorRangeList;

class StaticColorRanges : public ColorRanges
{
protected:
    const StaticColorRangeList ranges;

public:
    explicit StaticColorRanges(StaticColorRangeList &r) : ranges(r) {}
    int numPlanes() const override { return ranges.size(); }
    ColorVal min(int p) const override { if (p >= numPlanes()) return 0; assert(p<numPlanes()); return ranges[p].first; }
    ColorVal max(int p) const override { if (p >= numPlanes()) return 0; assert(p<numPlanes()); return ranges[p].second; }
};

const ColorRanges *getRanges(const Image &image);

class DupColorRanges : public ColorRanges {
protected:
    const ColorRanges *ranges;
public:
    explicit DupColorRanges(const ColorRanges *rangesIn) : ranges(rangesIn) {}

    int numPlanes() const override { return ranges->numPlanes(); }
    ColorVal min(int p) const override { return ranges->min(p); }
    ColorVal max(int p) const override { return ranges->max(p); }
    void minmax(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv) const override { ranges->minmax(p,pp,minv,maxv); }
    void snap(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv, ColorVal &v) const override { ranges->snap(p,pp,minv,maxv,v); }
    bool isStatic() const override { return ranges->isStatic(); }
    const ColorRanges* previous() const override { return ranges; }
};

const ColorRanges *dupRanges(const ColorRanges *ranges);
