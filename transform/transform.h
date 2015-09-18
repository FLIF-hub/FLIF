#ifndef _TRANSFORM_H_
#define _TRANSFORM_H_

#include "../image/image.h"
#include "../image/color_range.h"
#include "../maniac/rac.h"
#include "../flif_config.h"
#include "../flif.h"


class Transform {
protected:

public:
    virtual ~Transform() {};

    // On encode: init, process, save, meta, data, <processing>
    // On decode: init,          load, meta,       <processing>, invData           ( + optional configure anywhere)

    bool virtual init(const ColorRanges *srcRanges) { return true; }
    void virtual configure(const int setting) { }
    bool virtual process(const ColorRanges *srcRanges, const Image &image) { return true; };
    void virtual load(const ColorRanges *srcRanges, RacIn &rac) {};
    void virtual save(const ColorRanges *srcRanges, RacOut &rac) const {};
    const ColorRanges virtual *meta(Image& image, const ColorRanges *srcRanges) { return new DupColorRanges(srcRanges); }
    void virtual data(Image& image) const {}
    void virtual invData(Image& image) const {}

};

#endif
