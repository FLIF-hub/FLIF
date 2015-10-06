#pragma once

#include "transform.h"
#include <string>

template <typename IO>
Transform<IO> *create_transform(std::string desc);
