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

#include <vector>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <memory>
#include "crc32k.hpp"

#include "../io.hpp"
#include "../config.h"
#include "../compiler-specific.hpp"


#include "../../extern/lodepng.h"

#ifdef SUPPORT_HDR
typedef int32_t ColorVal;  // used in computations
#else
typedef int16_t ColorVal;  // at most 9-bit numbers + sign
#endif

typedef uint8_t ColorVal_intern_8;
typedef uint16_t ColorVal_intern_16u;
typedef int16_t ColorVal_intern_16;
typedef int32_t ColorVal_intern_32;

#ifdef USE_SIMD
#ifdef _MSC_VER
//MSVC does not support vector_size or any equivalent vector type
//using wrappers for x86 SSE2 intrinsics
#include <emmintrin.h>
#define VCALL __vectorcall //calling convention for passing arguments through registers
struct FourColorVals {
    __m128i vec;
    FourColorVals() : vec() {}
    FourColorVals(__m128i &v) : vec(v) {}
    FourColorVals(int v) : vec(_mm_set1_epi32(v)) {}
    FourColorVals(int v1, int v2, int v3, int v4): vec(_mm_set_epi32(v1, v2, v3, v4)) {}
    FourColorVals(const int32_t *p) {
        assert(((uintptr_t)p & 15) == 0);//assert aligned
        vec = _mm_load_si128((__m128i*)p);
    }    
    FourColorVals(const uint16_t *p) {
        assert(((uintptr_t)p & 7) == 0);//assert aligned
        vec = _mm_unpacklo_epi16(_mm_loadl_epi64((__m128i*)p),_mm_setzero_si128());
    }
    FourColorVals(const uint8_t *p) : vec() {}
    FourColorVals(const int16_t *p) : vec() {}
    void store(int32_t *p) const{
        assert(((uintptr_t)p & 15) == 0);//assert aligned
        _mm_store_si128((__m128i*)p,vec);
    }    
    void store(uint16_t *p) const{
        assert(((uintptr_t)p & 7) == 0);//assert aligned
        //_mm_packus_epi32 (pack with unsigned saturation) is not in SSE2 (2001) for some reason, requires SSE 4.1 (2007)
        //_mm_storel_epi64((__m128i*)p,_mm_packus_epi32 (a,a));

        //a:    AAAABBBBCCCCDDDD  input vector
        //slli: AA__BB__CC__DD__  bitshift left by 16
        //srli: __________AA__BB  byteshift right by 10
        //_or_: AA__BB__CCAADDBB  OR together
        //shuf: AA__BB__AABBCCDD  reshuffle low half: {[2], [0], [3], [1]} : 10 00 11 01 : 0x8D (I may have gotten this wrong)
        //storel:       AABBCCDD  store low half
        __m128i shifted = _mm_slli_epi32(vec,16);
        _mm_storel_epi64((__m128i*)p,_mm_shufflelo_epi16(_mm_or_si128(shifted,_mm_srli_si128(shifted,10)),0x8D));
    }
    void store(int16_t *p) const {}
    void store(uint8_t *p) const {}
    int operator[](int i) const{ return vec.m128i_i32[i]; }
    FourColorVals VCALL operator+(FourColorVals b) { return FourColorVals(_mm_add_epi32(vec,b.vec)); }
    FourColorVals VCALL operator-(FourColorVals b) { return FourColorVals(_mm_sub_epi32(vec,b.vec)); }    
    FourColorVals VCALL operator-() { return FourColorVals(_mm_sub_epi32(_mm_setzero_si128(),vec)); }
    FourColorVals VCALL operator>>(int n) { return FourColorVals(_mm_srai_epi32(vec,n)); }
    operator __m128i() { return vec; }
};
inline FourColorVals VCALL operator-(int a, FourColorVals b) { return FourColorVals(_mm_sub_epi32(_mm_set1_epi32(a),b.vec)); }
inline FourColorVals VCALL operator>(FourColorVals a, FourColorVals b) { return FourColorVals(_mm_cmpgt_epi32(a.vec,b.vec)); }
struct EightColorVals {
    __m128i vec;
    EightColorVals() : vec() {}
    EightColorVals(__m128i &v) : vec(v) {}
    EightColorVals(short v) : vec(_mm_set1_epi16(v)) {}
    EightColorVals(short v8, short v7, short v6, short v5, short v4, short v3, short v2, short v1): vec(_mm_set_epi16(v1, v2, v3, v4, v5, v6, v7, v8)) {}
    EightColorVals(const int16_t *p) {
        assert(((uintptr_t)p & 15) == 0);//assert aligned
        vec = _mm_load_si128((__m128i*)p);
    }
    EightColorVals(const uint8_t *p) {
        assert(((uintptr_t)p & 7) == 0);//assert aligned
        vec = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)p),_mm_setzero_si128());
    }
    EightColorVals(const uint16_t *p) : vec() {}
    EightColorVals(const int32_t *p) : vec() {}
    void store(int16_t *p) const{
        assert(((uintptr_t)p & 15) == 0);//assert aligned
        _mm_store_si128((__m128i*)p,vec);
    }    
    void store(uint8_t *p) const{
        assert(((uintptr_t)p & 7) == 0);//assert aligned
        _mm_storel_epi64((__m128i*)p,_mm_packus_epi16(vec,vec));
    }
    void store(int32_t *p) const {}
    void store(uint16_t *p) const {}
	short& operator[](int i) { return vec.m128i_i16[i]; }
	short operator[](int i) const { return vec.m128i_i16[i]; }
    EightColorVals VCALL operator+(EightColorVals b) { return EightColorVals(_mm_add_epi16(vec,b.vec)); }
    EightColorVals VCALL operator-(EightColorVals b) { return EightColorVals(_mm_sub_epi16(vec,b.vec)); }
    EightColorVals VCALL operator-() { return EightColorVals(_mm_sub_epi16(_mm_setzero_si128(),vec)); }
    EightColorVals VCALL operator>>(int n) { return EightColorVals(_mm_srai_epi16(vec,n)); }
    VCALL operator __m128i() { return vec; }
};
inline EightColorVals VCALL operator-(short a, EightColorVals b) { return EightColorVals(_mm_sub_epi16(_mm_set1_epi16(a),b.vec)); }
inline EightColorVals VCALL operator>(EightColorVals a, EightColorVals b) { return EightColorVals(_mm_cmpgt_epi16(a.vec,b.vec)); }
inline __m128i VCALL cond_op(__m128i a, __m128i b, __m128i cond) {
    __m128i ma = _mm_and_si128(a,cond);
    __m128i mb = _mm_and_si128(b,_mm_andnot_si128(_mm_setzero_si128(),cond));
    return _mm_or_si128(ma,mb);
}
#else
#define VCALL
#ifdef __clang__
// clang does not support scalar/vector operations with vector_size syntax
typedef int32_t  FourColorVals __attribute__ ((ext_vector_type (4)));
typedef int16_t  EightColorVals __attribute__ ((ext_vector_type (8)));  // only for 8-bit images (or at most 14-bit I guess)
#else
typedef int32_t  FourColorVals __attribute__ ((vector_size (16)));
typedef int16_t  EightColorVals __attribute__ ((vector_size (16)));  // only for 8-bit images (or at most 14-bit I guess)
#endif //__clang__
#endif //_MSC_VER
#endif //USE_SIMD


// It's a part of C++14. Following impl was taken from GotW#102
// (http://herbsutter.com/gotw/_102/).
// It should go into some common header one day.
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args &&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

struct PlaneVisitor;

//static const uint32_t pixelsizes[]={1,1,2,2,4,4,8,8,16,16,32,32,64,64,128,128,256,256,512,512,1024,1024,2048,2048,4096,4096,8192,8192,1<<14,1<<14,1<<15,1<<15,1<<16,1<<16};

class GeneralPlane {
public:
    virtual void set(const uint32_t r, const uint32_t c, const ColorVal x) =0;
    virtual ColorVal get(const uint32_t r, const uint32_t c) const =0;
#ifdef USE_SIMD
    virtual FourColorVals get4(const uint32_t pos) const =0;
    virtual void VCALL set4(const uint32_t pos, const FourColorVals x) =0;
    virtual EightColorVals get8(const uint32_t pos) const =0;
    virtual void VCALL set8(const uint32_t pos, const EightColorVals x) =0;
#endif

    virtual void prepare_zoomlevel(const int z) const =0;
    virtual ColorVal get_fast(uint32_t r, uint32_t c) const =0;
    virtual void set_fast(uint32_t r, uint32_t c, ColorVal x) =0;

    virtual bool is_constant() const { return false; }
    virtual ~GeneralPlane() { }
    virtual void set(const int z, const uint32_t r, const uint32_t c, const ColorVal x) =0;
    virtual ColorVal get(const int z, const uint32_t r, const uint32_t c) const =0;
    virtual void normalize_scale() {}
    virtual void accept_visitor(PlaneVisitor &v) =0;
    virtual uint32_t compute_crc32(uint32_t previous_crc32) =0;
    // access pixel by zoomlevel coordinate
    uint32_t zoom_rowpixelsize(int zoomlevel) const {
    //    return pixelsizes[zoomlevel+1];
        return 1<<((zoomlevel+1)/2);
    }
    uint32_t zoom_colpixelsize(int zoomlevel) const {
    //    return pixelsizes[zoomlevel];
        return 1<<((zoomlevel)/2);
    }
};

template <typename> class Plane;
class ConstantPlane;

struct PlaneVisitor {
    virtual void visit(Plane<ColorVal_intern_8>&) =0;
    virtual void visit(Plane<ColorVal_intern_16>&) =0;
#ifdef SUPPORT_HDR
    virtual void visit(Plane<ColorVal_intern_16u>&) =0;
    virtual void visit(Plane<ColorVal_intern_32>&) =0;
#endif
//    virtual void visit(ConstantPlane&) =0;
    virtual ~PlaneVisitor() {}
};

#define SCALED(x) (((x-1)>>scale)+1)
#ifdef USE_SIMD
// pad to a multiple of 8, leaving room for alignment
#define PAD(x) ((x) + 16)
#else
#define PAD(x) (x)
#endif


template <typename pixel_t> class Plane final : public GeneralPlane {
    std::vector<pixel_t> data_vec;
    pixel_t* data;
    const uint32_t width, height;
    int s;
    mutable uint32_t s_r, s_c;

public:
    Plane(uint32_t w, uint32_t h, ColorVal color=0, int scale = 0) : data_vec(PAD(SCALED(w)*SCALED(h)), color), width(SCALED(w)), height(SCALED(h)), s(scale) {
      // Align only when required. The emscripten port doesn't work with padded alignment and doesn't support SIMD, so
      // `USE_SIMD` is a good condition for alignment, for now.
#ifdef USE_SIMD
        //size_t space = data_vec.size()*sizeof(pixel_t);
        void *ptr = data_vec.data();
        //std::align (C++11) is not in GCC or Clang (the versions used by Travis-CI at least) for some stupid reason
        //data = static_cast<pixel_t*>(std::align(16,16,ptr,space));
        uintptr_t diff = (uintptr_t)ptr % 16;
        data = static_cast<pixel_t*>((diff == 0) ? ptr : ((char*)ptr) + (16-diff));
#else
        data = data_vec.data();
#endif
        assert(data != nullptr);
    }
    void clear() {
        data_vec.clear();
    }
    void set(const uint32_t r, const uint32_t c, const ColorVal x) override {
//        const uint32_t sr = r>>s, sc = c>>s;
        const uint32_t sr = r, sc = c;
//        assert(s==0);  // can also be used when using downscaled plane; in this case you have to make sure to use downscaled r,c !
        assert(sr<height); assert(sc<width);
        data[sr*width + sc] = x;
    }
    ColorVal get(const uint32_t r, const uint32_t c) const override ATTRIBUTE_HOT {
//        if (r >= height || r < 0 || c >= width || c < 0) {printf("OUT OF RANGE!\n"); return 0;}
//        const uint32_t sr = r>>s, sc = c>>s;
        const uint32_t sr = r, sc = c;
//        assert(s==0);  // can also be used when using downscaled plane; in this case you have to make sure to use downscaled r,c !
        assert(sr<height); assert(sc<width);
        return data[sr*width + sc];
    }
// get/set specialized for a particular zoomlevel
    void prepare_zoomlevel(const int z) const override {
        s_r = (zoom_rowpixelsize(z)>>s)*width;
        s_c = (zoom_colpixelsize(z)>>s);
    }
    ColorVal get_fast(uint32_t r, uint32_t c) const override {
        return data[r*s_r+c*s_c];
    }
    void set_fast(uint32_t r, uint32_t c, ColorVal x) override {
        data[r*s_r+c*s_c] = x;
    }
#ifdef USE_SIMD
// methods to just get all the values quickly
    FourColorVals get4(const uint32_t pos) const ATTRIBUTE_HOT {
#ifdef _MSC_VER
        assert(pos % 4 == 0);
        FourColorVals x(data + pos);
#else
        FourColorVals x {data[pos], data[pos+1], data[pos+2], data[pos+3]};
#endif
        return x;
    }
    void VCALL set4(const uint32_t pos, const FourColorVals x) override {
#ifdef _MSC_VER
        assert(pos % 4 == 0);
        x.store(data + pos);
#else
        data[pos]=x[0];
        data[pos+1]=x[1];
        data[pos+2]=x[2];
        data[pos+3]=x[3];
#endif
    }
    EightColorVals get8(const uint32_t pos) const ATTRIBUTE_HOT {
#ifdef _MSC_VER
        assert(pos % 8 == 0);
        EightColorVals x(data + pos);
#else
        EightColorVals x {(int16_t)data[pos], (int16_t)data[pos+1], (int16_t)data[pos+2], (int16_t)data[pos+3],
                          (int16_t)data[pos+4], (int16_t)data[pos+5], (int16_t)data[pos+6], (int16_t)data[pos+7]};
#endif
        return x;
    }
    void VCALL set8(const uint32_t pos, const EightColorVals x) override {
#ifdef _MSC_VER
        assert(pos % 8 == 0);
        x.store(data + pos);
#else
        data[pos]=x[0];
        data[pos+1]=x[1];
        data[pos+2]=x[2];
        data[pos+3]=x[3];
        data[pos+4]=x[4];
        data[pos+5]=x[5];
        data[pos+6]=x[6];
        data[pos+7]=x[7];
#endif
    }
#endif
    void set(const int z, const uint32_t r, const uint32_t c, const ColorVal x) override {
//        set(r*zoom_rowpixelsize(z),c*zoom_colpixelsize(z),x);
         data[(r*zoom_rowpixelsize(z)>>s)*width + (c*zoom_colpixelsize(z)>>s)] = x;
    }
    ColorVal get(const int z, const uint32_t r, const uint32_t c) const override {
//        return get(r*zoom_rowpixelsize(z),c*zoom_colpixelsize(z));
        return data[(r*zoom_rowpixelsize(z)>>s)*width + (c*zoom_colpixelsize(z)>>s)];
    }
    void normalize_scale() override { s = 0; }

    void accept_visitor(PlaneVisitor &v) override {
        v.visit(*this);
    }
    uint32_t compute_crc32(uint32_t previous_crc32) override {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        // temporarily make the buffer little endian (TODO: avoid this by modifying the crc to take the swapped bytes into account directly)
        if (sizeof(pixel_t) == 2) {
            for (pixel_t& x : data) x = swap16(x);
        } else if (sizeof(pixel_t) == 4) {
            for (pixel_t& x : data) x = swap(x);
        }
#endif
        uint32_t result = crc32_fast(&data[0], width*height*sizeof(pixel_t), previous_crc32);
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        // make the buffer big endian again
        if (sizeof(pixel_t) == 2) {
            for (pixel_t& x : data) x = swap16(x);
        } else if (sizeof(pixel_t) == 4) {
            for (pixel_t& x : data) x = swap(x);
        }
#endif
        return result;
    }
};

class ConstantPlane final : public GeneralPlane {
    ColorVal color;
public:
    ConstantPlane(ColorVal c) : color(c) {}
    void set(const uint32_t r, const uint32_t c, const ColorVal x) override {
        assert(x == color);
    }
    ColorVal get(const uint32_t r, const uint32_t c) const override {
        return color;
    }

    void prepare_zoomlevel(const int z) const override {}
    ColorVal get_fast(uint32_t r, uint32_t c) const override { return color; }
    void set_fast(uint32_t r, uint32_t c, ColorVal x) override { assert(x == color); }

#ifdef USE_SIMD
    FourColorVals get4(const uint32_t pos) const ATTRIBUTE_HOT {
        FourColorVals x {color,color,color,color};
        return x;
    }
    void VCALL set4(const uint32_t pos, const FourColorVals x) override {
        assert(x[0] == color);
        assert(x[1] == color);
        assert(x[2] == color);
        assert(x[3] == color);
    }
    EightColorVals get8(const uint32_t pos) const ATTRIBUTE_HOT {
        int16_t c = color;
        EightColorVals x {c,c,c,c,c,c,c,c};
        return x;
    }
    void VCALL set8(const uint32_t pos, const EightColorVals x) override {
        assert(x[0] == color);
        assert(x[1] == color);
        assert(x[2] == color);
        assert(x[3] == color);
        assert(x[4] == color);
        assert(x[5] == color);
        assert(x[6] == color);
        assert(x[7] == color);
    }
#endif
    bool is_constant() const override { return true; }

    void set(const int z, const uint32_t r, const uint32_t c, const ColorVal x) override {
        assert(x == color);
    }
    ColorVal get(const int z, const uint32_t r, const uint32_t c) const override {
        return color;
    }

    void accept_visitor(PlaneVisitor &v) override {
//        v.visit(*this);
        assert(false); // there should never be a need to visit a constant plane
    }
    uint32_t compute_crc32(uint32_t previous_crc32) override {
        uint16_t onepixel = color;
        return crc32_fast(&onepixel, 2, previous_crc32);
    }
};

template<typename plane_t>
void copy_row_range(plane_t &plane, const GeneralPlane &other, const uint32_t r, const uint32_t begin, const uint32_t end, const uint32_t stride = 1) {
    //assuming pixels are only ever copied from either a constant plane or a plane of the same type
    if (other.is_constant()) {
        const ConstantPlane &src = static_cast<const ConstantPlane&>(other);
        for(uint32_t c = begin; c < end; c+= stride) plane.set(r,c, src.get(r,c));
    }else {
        const plane_t &src = static_cast<const plane_t&>(other);
        for(uint32_t c = begin; c < end; c+= stride) plane.set(r,c, src.get(r,c));
    }
}

struct MetaData {
    char name[5];               // name of the chunk (every chunk is assumed to be unique, 4 ascii letters plus terminating 0)
    size_t length;              // length of the chunk contents
    std::vector<unsigned char> contents;
};

struct metadata_options {
    bool icc;
    bool exif;
    bool xmp;
};

class Image {
    std::unique_ptr<GeneralPlane> planes[5]; // Red/Y, Green/Co, Blue/Cg, Alpha, Frame-Lookback(animation only)
    uint32_t width, height;
    ColorVal minval,maxval;
    int num;
    int scale;
#ifdef SUPPORT_HDR
    int depth;
#else
    const int depth=8;
#endif

    Image(const Image& other) {
      // reuse implementation from assignment operator
      operator=(other);
    }


    Image& operator=(const Image& other) {
      width = other.width;
      height = other.height;
      minval = other.minval;
      maxval = other.maxval;
      num = other.num;
      scale = other.scale;
#ifdef SUPPORT_HDR
      depth = other.depth;
#endif
      metadata = other.metadata;
      palette = other.palette;
      frame_delay = other.frame_delay;
      col_begin = other.col_begin;
      col_end = other.col_end;
      seen_before = other.seen_before;
      fully_decoded = other.fully_decoded;
      clear();
      {
      int p=num;
      if (depth <= 8) {
        if (p>0) planes[0] = make_unique<Plane<ColorVal_intern_8>>(width, height, 0, scale); // R,Y
        if (p>1) planes[1] = make_unique<Plane<ColorVal_intern_16>>(width, height, 0, scale); // G,I
        if (p>2) planes[2] = make_unique<Plane<ColorVal_intern_16>>(width, height, 0, scale); // B,Q
        if (p>3) planes[3] = make_unique<Plane<ColorVal_intern_8>>(width, height, 0, scale); // A
#ifdef SUPPORT_HDR
      } else {
        if (p>0) planes[0] = make_unique<Plane<ColorVal_intern_16u>>(width, height, 0, scale); // R,Y
        if (p>1) planes[1] = make_unique<Plane<ColorVal_intern_32>>(width, height, 0, scale); // G,I
        if (p>2) planes[2] = make_unique<Plane<ColorVal_intern_32>>(width, height, 0, scale); // B,Q
        if (p>3) planes[3] = make_unique<Plane<ColorVal_intern_16u>>(width, height, 0, scale); // A
#endif
      }
      if (p>4) planes[4] = make_unique<Plane<ColorVal_intern_8>>(width, height, 0, scale); // FRA
      }
      for(int p=0; p<num; p++)
          for (uint32_t r=0; r<SCALED(height); r++)
             for (uint32_t c=0; c<SCALED(width); c++)
                 set(p,r,c,other.operator()(p,r,c));

      return *this;
    }



public:
    bool palette;
    int frame_delay;
    bool alpha_zero_special;
    std::vector<uint32_t> col_begin;
    std::vector<uint32_t> col_end;
    int seen_before;
    bool fully_decoded;
    std::vector<MetaData> metadata;

    Image(uint32_t width, uint32_t height, ColorVal min, ColorVal max, int planes, int s=0) : scale(s) {
        init(width, height, min, max, planes);
    }

    Image(int s=0) : scale(s) {
      width = height = 0;
      minval = maxval = 0;
      num = 0;
      frame_delay = 0;
      fully_decoded = false;
#ifdef SUPPORT_HDR
      depth = 0;
#endif
      palette = false;
      alpha_zero_special = true;
      seen_before = 0;
    }

    // move constructor
    Image(Image&& other) {
      // reuse implementation from assignment operator
      operator=(std::move(other));
    }
    Image& operator=(Image&& other) {
      width = other.width;
      height = other.height;
      minval = other.minval;
      maxval = other.maxval;
      num = other.num;
      scale = other.scale;
      fully_decoded = other.fully_decoded;
      for (int p=0; p<num; p++) planes[p] = std::move(other.planes[p]);
      frame_delay = other.frame_delay;
#ifdef SUPPORT_HDR
      depth = other.depth;
      other.depth = 0;
#endif
      metadata = other.metadata;

      other.width = other.height = 0;
      other.minval = other.maxval = 0;
      other.num = 0;
      other.frame_delay = 0;
      other.fully_decoded = false;

      palette = other.palette;
      alpha_zero_special = other.alpha_zero_special;
      col_begin = std::move(other.col_begin);
      col_end = std::move(other.col_end);
      seen_before = other.seen_before;

      other.palette = false;
      other.alpha_zero_special = true;
      other.seen_before = 0;
      return *this;
    }

    // downsampling copy constructor
    Image(const Image& other, int new_w, int new_h) {
      width = new_w;
      height = new_h;
      minval = other.minval;
      maxval = other.maxval;
      num = other.num;
      scale = 0;
#ifdef SUPPORT_HDR
      depth = other.depth;
#endif
      metadata = other.metadata;
      palette = other.palette;
      frame_delay = other.frame_delay;
      col_begin = other.col_begin;
      col_end = other.col_end;
      seen_before = other.seen_before;
      fully_decoded = other.fully_decoded;
      clear();
      {
      int p=num;
      if (depth <= 8) {
        if (p>0) planes[0] = make_unique<Plane<ColorVal_intern_8>>(width, height, 0, scale); // R,Y
        if (p>1) planes[1] = make_unique<Plane<ColorVal_intern_16>>(width, height, 0, scale); // G,I
        if (p>2) planes[2] = make_unique<Plane<ColorVal_intern_16>>(width, height, 0, scale); // B,Q
        if (p>3) planes[3] = make_unique<Plane<ColorVal_intern_8>>(width, height, 0, scale); // A
#ifdef SUPPORT_HDR
      } else {
        if (p>0) planes[0] = make_unique<Plane<ColorVal_intern_16u>>(width, height, 0, scale); // R,Y
        if (p>1) planes[1] = make_unique<Plane<ColorVal_intern_32>>(width, height, 0, scale); // G,I
        if (p>2) planes[2] = make_unique<Plane<ColorVal_intern_32>>(width, height, 0, scale); // B,Q
        if (p>3) planes[3] = make_unique<Plane<ColorVal_intern_16u>>(width, height, 0, scale); // A
#endif
      }
      if (p>4) planes[4] = make_unique<Plane<ColorVal_intern_8>>(width, height, 0, scale); // FRA
      }
      // this is stupid downsampling
      // TODO: replace this with more accurate downscaling
      for(int p=0; p<num; p++)
          for (uint32_t r=0; r<height; r++)
             for (uint32_t c=0; c<width; c++)
                 set(p,r,c,other.operator()(p,r*other.height/height,c*other.width/width));
    }

    bool init(uint32_t w, uint32_t h, ColorVal min, ColorVal max, int p) {
      width = w;
      height = h;
      minval = min;
      maxval = max;
      num = p;
      seen_before = -1;
#ifdef SUPPORT_HDR
      if (max < 256) depth=8; else depth=16;
#else
      assert(max<256);
#endif
      frame_delay=0;
      palette=false;
      alpha_zero_special=true;
      assert(min == 0);
      assert(max < (1<<depth));
      assert(p <= 4);
      fully_decoded=false;

      clear();
      try {
      col_begin.clear();
      col_begin.resize(height,0);
      col_end.clear();
      col_end.resize(height,width);
      if (depth <= 8) {
        if (p>0) planes[0] = make_unique<Plane<ColorVal_intern_8>>(width, height, 0, scale); // R,Y
        if (p>1) planes[1] = make_unique<Plane<ColorVal_intern_16>>(width, height, 0, scale); // G,I
        if (p>2) planes[2] = make_unique<Plane<ColorVal_intern_16>>(width, height, 0, scale); // B,Q
        if (p>3) planes[3] = make_unique<Plane<ColorVal_intern_8>>(width, height, 0, scale); // A
#ifdef SUPPORT_HDR
      } else {
        if (p>0) planes[0] = make_unique<Plane<ColorVal_intern_16u>>(width, height, 0, scale); // R,Y
        if (p>1) planes[1] = make_unique<Plane<ColorVal_intern_32>>(width, height, 0, scale); // G,I
        if (p>2) planes[2] = make_unique<Plane<ColorVal_intern_32>>(width, height, 0, scale); // B,Q
        if (p>3) planes[3] = make_unique<Plane<ColorVal_intern_16u>>(width, height, 0, scale); // A
#endif
      }
      if (p>4) planes[4] = make_unique<Plane<ColorVal_intern_8>>(width, height, 0, scale); // FRA
      }
      catch (std::bad_alloc& ba) {
        e_printf("Error: could not allocate enough memory for image data.\n");
        return false;
      }
      return true;
    }

    // Copy constructor is private to avoid mistakes.
    // This function can be used if copies are necessary.
    Image clone()
    {
      return *this;
    }
    void normalize_scale() {
//      v_printf(3,"%ix%i -> ",width,height);
      width = SCALED(width);
      height = SCALED(height);
      scale = 0;
//      v_printf(3,"%ix%i\n",width,height);
      col_begin.clear();
      col_begin.resize(height,0);
      col_end.clear();
      col_end.resize(height,width);
      for(int p = 0; p < num; p++)
          planes[p]->normalize_scale();
    }

    void clear() {
        for (int p=0; p<5; p++) planes[p].reset(nullptr);
    }
    void reset() {
        clear();
        init(0,0,0,0,0);
    }
    bool uses_alpha() const {
        assert(depth == 8 || depth == 16);
        if (num<4) return false;
        for (uint32_t r=0; r<height; r++)
           for (uint32_t c=0; c<width; c++)
              if (operator()(3,r,c) < (1<<depth)-1) return true;
        return false; // alpha plane is completely opaque, so it is useless
    }
    bool uses_color() const {
        assert(depth == 8 || depth == 16);
        if (num<3) return false;
        for (uint32_t r=0; r<height; r++)
           for (uint32_t c=0; c<width; c++)
              if (operator()(0,r,c) != operator()(1,r,c) || operator()(0,r,c) != operator()(2,r,c)) return true;
        return false; // R=G=B for all pixels, so image is grayscale
    }
    void drop_alpha() {
        if (num<4) return;
        assert(num==4);
        planes[3].reset(nullptr);
        num=3;
    }
    void make_invisible_rgb_black() {
        if (num<4) return;
        for (uint32_t r=0; r<height; r++)
           for (uint32_t c=0; c<width; c++)
              if (operator()(3,r,c) == 0) {
                set(0,r,c,0);
                set(1,r,c,0);
                set(2,r,c,0);
              }
    }
    void drop_color() {
        if (num<2) return;
        assert(num==3);
        planes[1].reset(nullptr);
        planes[2].reset(nullptr);
        num=1;
    }
    void drop_frame_lookbacks() {
        assert(num==5);
        planes[4].reset(nullptr);
        num=4;
    }
    void make_constant_plane(const int p, const ColorVal val) {
      if (p>3 || p<0) return;
      planes[p].reset(nullptr);
      planes[p] = make_unique<ConstantPlane>(val);
    }
    void undo_make_constant_plane(const int p) {
      if (p>3 || p<0) return;
      if (!planes[p]->is_constant()) return;
      ColorVal val = operator()(p,0,0);
      planes[p].reset(nullptr);
      if (depth <= 8) {
        if (p==0) planes[0] = make_unique<Plane<ColorVal_intern_8>>(width, height, val, scale); // R,Y
        if (p==1) planes[1] = make_unique<Plane<ColorVal_intern_16>>(width, height, val, scale); // G,I
        if (p==2) planes[2] = make_unique<Plane<ColorVal_intern_16>>(width, height, val, scale); // B,Q
        if (p==3) planes[3] = make_unique<Plane<ColorVal_intern_8>>(width, height, val, scale); // A
#ifdef SUPPORT_HDR
      } else {
        if (p==0) planes[0] = make_unique<Plane<ColorVal_intern_16u>>(width, height, val, scale); // R,Y
        if (p==1) planes[1] = make_unique<Plane<ColorVal_intern_32>>(width, height, val, scale); // G,I
        if (p==2) planes[2] = make_unique<Plane<ColorVal_intern_32>>(width, height, val, scale); // B,Q
        if (p==3) planes[3] = make_unique<Plane<ColorVal_intern_16u>>(width, height, val, scale); // A
#endif
      }
    }
    void ensure_chroma() {
        switch(num) {
            case 1:
              make_constant_plane(1,((1<<depth)-1));
            case 2:
              make_constant_plane(2,((1<<depth)-1));
              num=3;
            default:
              assert(num>=3);
        }
    }
    void ensure_alpha() {
        ensure_chroma();
        switch(num) {
            case 3:
              make_constant_plane(3,255);
              num=4;
            default:
              assert(num>=4);
        }
    }
    void ensure_frame_lookbacks() {
        if (num < 5) {
            ensure_alpha();
            planes[4] = make_unique<Plane<ColorVal_intern_8>>(width, height, 0, scale);
            num=5;
        }
    }

#ifdef HAS_ENCODER
    bool load(const char *name, metadata_options &options);
#endif
    bool save(const char *name) const;

    // access pixel by coordinate
    ColorVal operator()(const int p, const uint32_t r, const uint32_t c) const ATTRIBUTE_HOT {
      assert(p>=0);
      assert(p<num);
      return planes[p]->get(r,c);
    }
    void set(int p, uint32_t r, uint32_t c, ColorVal x) {
      assert(p>=0);
      assert(p<num);
      planes[p]->set(r,c,x);
    }

    int numPlanes() const { return num; }
    ColorVal min(int) const { return minval; }
    ColorVal max(int) const { return maxval; }
    uint32_t rows() const { return height; }
    uint32_t cols() const { return width; }
    int getscale() const { return scale; }

    // access pixel by zoomlevel coordinate
    uint32_t zoom_rowpixelsize(int zoomlevel) const {
        return 1<<((zoomlevel+1)/2);
    }
    uint32_t zoom_colpixelsize(int zoomlevel) const {
        return 1<<((zoomlevel)/2);
    }

    uint32_t rows(int zoomlevel) const {
        return 1+(rows()-1)/zoom_rowpixelsize(zoomlevel);
    }
    uint32_t cols(int zoomlevel) const {
        return 1+(cols()-1)/zoom_colpixelsize(zoomlevel);
    }
    int zooms() const {
        int z = 0;
        while (zoom_rowpixelsize(z) < rows() || zoom_colpixelsize(z) < cols()) z++;
        return z;
    }
    ColorVal operator()(int p, int z, uint32_t rz, uint32_t cz) const ATTRIBUTE_HOT {
        assert(p>=0);
        assert(p<num);
        return planes[p]->get(z,rz,cz);
    }
    void set(int p, int z, uint32_t rz, uint32_t cz, ColorVal x) {
        assert(p>=0);
        assert(p<num);
        planes[p]->set(z,rz,cz,x);
    }

    GeneralPlane& getPlane(int p) {
        assert(p>=0);
        assert(p<num);
        return *planes[p];
    }
    const GeneralPlane& getPlane(int p) const{
        assert(p>=0);
        assert(p<num);
        return *planes[p];
    }

    ColorVal getFRA(const uint32_t r, const uint32_t c) {
        return static_cast<Plane<ColorVal_intern_8>&>(*planes[4]).get(r,c);
    }

    ColorVal getFRA(const uint32_t z, const uint32_t r, const uint32_t c) {
        return static_cast<Plane<ColorVal_intern_8>&>(*planes[4]).get(z,r,c);
    }

    int getDepth() const {
        return depth;
    }

    uint32_t checksum() {
          uint32_t crc = (width << 16) + height;
          for(int p=0; p<num; p++)
            crc = planes[p]->compute_crc32(crc);
          return crc;
/*
          uint_fast32_t crc=0;
          crc32k_transform(crc,width & 255);
          crc32k_transform(crc,width / 256);
          crc32k_transform(crc,height & 255);
          crc32k_transform(crc,height / 256);

          for(int p=0; p<num; p++)
            for (uint32_t r=0; r<height; r++)
               for (uint32_t c=0; c<width; c++) {
                  ColorVal d = operator()(p,r,c);
                  crc32k_transform(crc, d & 255);
                  crc32k_transform(crc, (d >> 8) & 255);
               }
//          printf("Computed checksum: %X\n", (~crc & 0xFFFFFFFF));
          return (~crc & 0xFFFFFFFF);
*/
    }
    void abort_decoding() {
        width = 0; // this is used to signal the decoder to stop
    }

    bool get_metadata(const char * chunkname, unsigned char ** data, size_t * length) const {
        for(size_t i=0; i<metadata.size(); i++) {
            if (!strncmp(metadata[i].name, chunkname, 4)) {
                *data = NULL;
                *length = 0;
//                lodepng_zlib_decompress(data, length, metadata[i].contents.data(), metadata[i].length, &lodepng_default_decompress_settings);
                lodepng_inflate(data, length, metadata[i].contents.data(), metadata[i].length, &lodepng_default_decompress_settings);
                return true;
            }
        }
        return false;  // metadata not found
    }
    void free_metadata(unsigned char * data) const {
        if(data) {
#ifdef LODEPNG_COMPILE_ALLOCATORS
            free(data);
#else
            lodepng_free(data);
#endif // !LODEPNG_COMPILE_ALLOCATORS            
        }
    }
    void set_metadata(const char * chunkname, const unsigned char * data, size_t length) {
        MetaData foo;
        strcpy(foo.name, chunkname);
        unsigned char * compressed = NULL;
        size_t compressed_length = 0;
//        lodepng_zlib_compress(&compressed, &compressed_length, data, length, &lodepng_default_compress_settings);
        lodepng_deflate(&compressed, &compressed_length, data, length, &lodepng_default_compress_settings);
        foo.contents.resize(compressed_length);
        memcpy(foo.contents.data(), compressed, compressed_length);
        free(compressed);
        foo.length = compressed_length;
        metadata.push_back(foo);
    }

};

typedef std::vector<Image>    Images;
