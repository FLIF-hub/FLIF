/*
 FLIF - Free Lossless Image Format
 Copyright (C) 2010-2015  Jon Sneyers & Pieter Wuille, LGPL v3+

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
