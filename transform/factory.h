#ifndef FLIF_TRANSFORM_FACTORY_H
#define FLIF_TRANSFORM_FACTORY_H

#include "transform.h"
#include <string>

template <typename IO>
Transform<IO> *create_transform(std::string desc);

#endif
