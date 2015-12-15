#include <string>

#include "transform.hpp"

#include "yiq.hpp"
#include "bounds.hpp"
#include "colorbuckets.hpp"
#include "palette.hpp"
#include "palette_A.hpp"
#include "palette_C.hpp"
#include "frameshape.hpp"
#include "framedup.hpp"
#include "framecombine.hpp"

template <typename IO>
Transform<IO> *create_transform(std::string desc)
{
    if (desc == "YCoCg")
        return new TransformYIQ<IO>();
    if (desc == "Bounds")
        return new TransformBounds<IO>();
    if (desc == "Color_Buckets")
        return new TransformCB<IO>();
    if (desc == "Palette")
        return new TransformPalette<IO>();
    if (desc == "Palette_Alpha")
        return new TransformPaletteA<IO>();
    if (desc == "Channel_Compact")
        return new TransformPaletteC<IO>();
    if (desc == "Frame_Shape")
        return new TransformFrameShape<IO>();
    if (desc == "Duplicate_Frame")
        return new TransformFrameDup<IO>();
    if (desc == "Frame_Lookback")
        return new TransformFrameCombine<IO>();
    return NULL;
}

template Transform<FileIO> *create_transform(std::string desc);
template Transform<BlobReader> *create_transform(std::string desc);
template Transform<BlobIO> *create_transform(std::string desc);
