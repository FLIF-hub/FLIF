#pragma once

#include "transform.hpp"
#include <string>

template <typename IO>
std::unique_ptr<Transform<IO>> create_transform(const std::string &desc);
