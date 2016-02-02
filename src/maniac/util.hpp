/*
FLIF - Free Lossless Image Format

Copyright 2010-2016, Jon Sneyers & Pieter Wuille

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once

#include <cstdint>
#include "bit.hpp"

namespace maniac {
namespace util {

static inline int ilog2(uint32_t l) {
    if (l == 0) { return 0; }
//    return sizeof(unsigned int) * 8 - __builtin_clz(l) - 1;
    return sizeof(unsigned int) * 8 - 1 - __builtin_clz(l);
}

void indent(int n);


} // namespace util
} // namespace maniac
