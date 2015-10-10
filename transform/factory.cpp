#include <string>

#include "transform.h"

#include "yiq.h"
#include "bounds.h"
#include "colorbuckets.h"
#include "palette.h"
#include "palette_A.h"
#include "frameshape.h"
#include "framedup.h"
#include "framecombine.h"

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
