#ifndef FLIF_TRANSFORM_H
#define FLIF_TRANSFORM_H

#include "../image/image.h"
#include "../image/color_range.h"
#include "../maniac/rac.h"
#include "../flif_config.h"
#include "../common.h"


template <typename IO>
class Transform {
protected:

public:
    virtual ~Transform() {};

    // On encode: init, process, save, meta, data, <processing>
    // On decode: init,          load, meta,       <processing>, invData           ( + optional configure anywhere)

    bool virtual init(const ColorRanges *srcRanges) { return true; }
    void virtual configure(const int setting) { }
    bool virtual process(const ColorRanges *srcRanges, const Images &images) { return true; };
    void virtual load(const ColorRanges *srcRanges, RacIn<IO> &rac) {};
    void virtual save(const ColorRanges *srcRanges, RacOut<IO> &rac) const {};
    const ColorRanges virtual *meta(Images& images, const ColorRanges *srcRanges) { return new DupColorRanges(srcRanges); }
    void virtual data(Images& images) const {}
    void virtual invData(Images& images) const {}

};

#endif
