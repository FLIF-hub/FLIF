#include <string>

#include "transform.hpp"

#include "ycocg.hpp"
//#include "yc1c2.hpp"
#include "permute.hpp"
#include "bounds.hpp"
#include "colorbuckets.hpp"
#include "palette.hpp"
#include "palette_A.hpp"
#include "palette_C.hpp"
//#include "palette_CoCg.hpp"
#ifdef SUPPORT_ANIMATION
#include "frameshape.hpp"
#include "framedup.hpp"
#include "framecombine.hpp"
#endif
//#include "dct.hpp"

template <typename IO>
std::unique_ptr<Transform<IO>> create_transform(const std::string &desc) {
    if (desc == "YCoCg")
        return make_unique<TransformYCoCg<IO>>();
// use this if you just want to quickly try YC1C2
//        return new TransformYCC<IO>();
    if (desc == "Bounds")
        return make_unique<TransformBounds<IO>>();
    if (desc == "PermutePlanes")
        return make_unique<TransformPermute<IO>>();
    if (desc == "Color_Buckets")
        return make_unique<TransformCB<IO>>();
    if (desc == "Palette")
        return make_unique<TransformPalette<IO>>();
    if (desc == "Palette_Alpha")
        return make_unique<TransformPaletteA<IO>>();
//    if (desc == "Palette_Chroma")  // turned out to be useless
//        return make_unique<TransformPaletteCoCg<IO>>();
    if (desc == "Channel_Compact")
        return make_unique<TransformPaletteC<IO>>();
#ifdef SUPPORT_ANIMATION
    if (desc == "Frame_Shape")
        return make_unique<TransformFrameShape<IO>>();
    if (desc == "Duplicate_Frame")
        return make_unique<TransformFrameDup<IO>>();
    if (desc == "Frame_Lookback")
        return make_unique<TransformFrameCombine<IO>>();
#endif
//    if (desc == "DCT")
//        return make_unique<TransformDCT<IO>>();
    return NULL;
}

template std::unique_ptr<Transform<FileIO>> create_transform(const std::string &desc);
template std::unique_ptr<Transform<BlobReader>> create_transform(const std::string &desc);
template std::unique_ptr<Transform<BlobIO>> create_transform(const std::string &desc);
