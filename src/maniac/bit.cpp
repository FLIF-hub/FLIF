#ifdef _MSC_VER

#include <intrin.h>
int __builtin_clz(unsigned int value)
{
	unsigned long r;
	_BitScanReverse(&r, value);
	return (31 - r);
}

#endif
