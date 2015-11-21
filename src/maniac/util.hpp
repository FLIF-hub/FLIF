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

template<typename I> void static swap(I& a, I& b)
{
    I c = a;
    a = b;
    b = c;
}

template<typename I> I static median3(I a, I b, I c)
{
    if (a<b) swap(a,b);
    if (b<c) swap(b,c);
    if (a<b) swap(a,b);
    return b;
}

} // namespace util
} // namespace maniac
