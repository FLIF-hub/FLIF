#pragma once

#include <stdlib.h>

// define endianess and some integer data types
#if defined(_MSC_VER) || defined(__MINGW32__)
  typedef unsigned __int8  uint8_t;
  typedef unsigned __int16 uint16_t;
  typedef unsigned __int32 uint32_t;
  typedef   signed __int32  int32_t;

  #define __ORDER_BIG_ENDIAN__ 4321
  #define __ORDER_LITTLE_ENDIAN__ 1234
  #define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__

  #include <xmmintrin.h>
  #ifdef __MINGW32__
    #define PREFETCH(location) __builtin_prefetch(location)
  #else
    #define PREFETCH(location) _mm_prefetch(location, _MM_HINT_T0)
  #endif
#else
  // uint8_t, uint32_t, in32_t
  #include <stdint.h>
  // defines __BYTE_ORDER as __LITTLE_ENDIAN or __BIG_ENDIAN
  #include <sys/param.h>

  #ifdef __GNUC__
    #define PREFETCH(location) __builtin_prefetch(location)
  #else
    #define PREFETCH(location) ;
  #endif
#endif


/// zlib's CRC32 polynomial
const uint32_t Polynomial = 0xEDB88320;

/// swap endianess
static inline uint32_t swap(uint32_t x)
{
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap32(x);
#else
  return (x >> 24) |
        ((x >>  8) & 0x0000FF00) |
        ((x <<  8) & 0x00FF0000) |
         (x << 24);
#endif
}

/// swap endianess
inline uint16_t swap16(uint16_t x)
{
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap16(x);
#else
  return (x >> 8) |
         (x << 8);
#endif
}

uint32_t crc32_fast(const void* data, size_t length, uint32_t previousCrc32 = 0);
