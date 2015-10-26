#include <string>

#include "transform.hpp"

#include "yiq.hpp"
#include "bounds.hpp"
#include "colorbuckets.hpp"
#include "palette.hpp"
#include "palette_A.hpp"
#include "frameshape.hpp"
#include "framedup.hpp"
#include "framecombine.hpp"

template <typename IO>
Transform<IO> *create_transform(std::string desc)
{
    if (desc == "YIQ")
        return new TransformYIQ<IO>();
    if (desc == "BND")
        return new TransformBounds<IO>();
    if (desc == "ACB")
        return new TransformCB<IO>();
    if (desc == "PLT")
        return new TransformPalette<IO>();
    if (desc == "PLA")
        return new TransformPaletteA<IO>();
    if (desc == "FRS")
        return new TransformFrameShape<IO>();
    if (desc == "DUP")
        return new TransformFrameDup<IO>();
    if (desc == "FRA")
        return new TransformFrameCombine<IO>();
    return NULL;
}

template Transform<FileIO> *create_transform(std::string desc);
template Transform<BlobReader> *create_transform(std::string desc);
template Transform<BlobIO> *create_transform(std::string desc);
