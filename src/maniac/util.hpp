#pragma once

#include <cstdint>
#include "bit.hpp"

namespace maniac {
namespace util {

static inline int ilog2(uint32_t l) {
    if (l == 0) { return 0; }
    return sizeof(unsigned int) * 8 - __builtin_clz(l) - 1;
}

void indent(int n);


} // namespace util
} // namespace maniac
