#pragma once
#include <stdio.h>

// SDL2 for Visual Studio C++ 2015

#if _MSC_VER >= 1900
FILE _iob[] = {*stdin, *stdout, *stderr};
extern "C" FILE * __cdecl __iob_func(void)
{
	return _iob;
}
void HackToReferencePrintEtc()
{
	fprintf(stderr, "");
}
#endif
