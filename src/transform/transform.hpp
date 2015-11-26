#pragma once

#include "../image/image.hpp"
#include "../image/color_range.hpp"
#include "../maniac/rac.hpp"
#include "../flif_config.h"


template <typename IO>
class Transform {
protected:

public:
    virtual ~Transform() {};

    // On encode: init, process, save, meta, data, <encoding>
    // On decode: init,          load, meta,       <decoding>, invData           ( + optional configure anywhere)
    // Progressive decode: init, load, meta,       <decoding>, invData, <render>, data,
    //                                             <decoding>, invData, <render>, data,
    //                                             ...
    //                                             <decoding>, invData


    bool virtual init(const ColorRanges *) { return true; }
    bool virtual undo_redo_during_decode() { return true; }
    void virtual configure(const int) { }
    bool virtual load(const ColorRanges *, RacIn<IO> &) { return true; };
#ifdef HAS_ENCODER
    bool virtual process(const ColorRanges *, const Images &) { return true; };
    void virtual save(const ColorRanges *, RacOut<IO> &) const {};
    void virtual data(Images&) const {}
#endif
    const ColorRanges virtual *meta(Images&, const ColorRanges *srcRanges) { return new DupColorRanges(srcRanges); }
    void virtual invData(Images&) const {}

};
