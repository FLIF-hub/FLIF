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

Transform *create_transform(std::string desc)
{
    if (desc == "YIQ")
        return new TransformYIQ();
    if (desc == "BND")
        return new TransformBounds();
    if (desc == "ACB")
        return new TransformCB();
    if (desc == "PLT")
        return new TransformPalette();
    if (desc == "PLA")
        return new TransformPaletteA();
    if (desc == "FRS")
        return new TransformFrameShape();
    if (desc == "DUP")
        return new TransformFrameDup();
    if (desc == "FRA")
        return new TransformFrameCombine();
    return NULL;
}
