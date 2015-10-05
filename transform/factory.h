#ifndef FLIF_TRANSFORM_FACTORY_H
#define FLIF_TRANSFORM_FACTORY_H

#include "transform.h"
#include <string>

Transform *create_transform(std::string desc);

#endif
