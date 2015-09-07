#include <string>

#include "transform.h"

#include "yiq.h"
#include "bounds.h"
#include "colorbuckets.h"
#include "palette.h"

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
    return NULL;
}
