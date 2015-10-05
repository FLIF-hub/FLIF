#ifndef FLIF_UTIL_H
#define FLIF_UTIL_H 1

#include <stdint.h>

extern const uint8_t log2_tab[1024];
int ilog2(uint32_t l);
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

//#define STATS 1


#endif
