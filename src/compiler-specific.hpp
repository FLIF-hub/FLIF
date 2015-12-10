#pragma once

#ifdef _MSC_VER
#define ATTRIBUTE_HOT
#else
#define ATTRIBUTE_HOT __attribute__ ((hot))
#endif