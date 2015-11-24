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

template<typename I> I static median3(I a, I b, I c)
{
    if (a < b) {
	if (b < c) {
          return b;
        } else {
          return a < c ? c : a;
        }
    } else {
       if (a < c) {
         return a;
       } else {
          return b < c ? c : b;
       }
    }
}

} // namespace util
} // namespace maniac
