#pragma once

#include "transform.hpp"
#include <string>

template <typename IO>
Transform<IO> *create_transform(std::string desc);
