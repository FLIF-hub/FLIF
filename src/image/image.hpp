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
#include <valarray>
#include <memory>
#include "crc32k.hpp"

#include "../io.hpp"
#include "../config.h"
#include "../compiler-specific.hpp"

typedef int32_t ColorVal;  // used in computations

typedef uint8_t ColorVal_intern_8;
typedef uint16_t ColorVal_intern_16u;
typedef int16_t ColorVal_intern_16;
typedef int32_t ColorVal_intern_32;

#ifdef USE_SIMD
#ifdef __clang__
// clang does not support scalar/vector operations with vector_size syntax
typedef ColorVal FourColorVals __attribute__ ((ext_vector_type (4)));
typedef int16_t  EightColorVals __attribute__ ((ext_vector_type (8)));  // only for 8-bit images (or at most 14-bit I guess)
#else
typedef ColorVal FourColorVals __attribute__ ((vector_size (16)));
typedef int16_t  EightColorVals __attribute__ ((vector_size (16)));  // only for 8-bit images (or at most 14-bit I guess)
#endif
#endif

// It's a part of C++14. Following impl was taken from GotW#102
// (http://herbsutter.com/gotw/_102/).
// It should go into some common header one day.
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args &&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

struct PlaneVisitor;


class GeneralPlane {
public:
    virtual void set(const uint32_t r, const uint32_t c, const ColorVal x) =0;
    virtual ColorVal get(const uint32_t r, const uint32_t c) const =0;
#ifdef USE_SIMD
    virtual FourColorVals get4(const uint32_t pos) const =0;
    virtual void set4(const uint32_t pos, const FourColorVals x) =0;
    virtual EightColorVals get8(const uint32_t pos) const =0;
    virtual void set8(const uint32_t pos, const EightColorVals x) =0;
#endif
    virtual bool is_constant() const { return false; }
    virtual ~GeneralPlane() { }
    virtual void set(const int z, const uint32_t r, const uint32_t c, const ColorVal x) =0;
    virtual ColorVal get(const int z, const uint32_t r, const uint32_t c) const =0;
    virtual void normalize_scale() {}
    virtual void check_equal(const ColorVal x, const uint32_t r, const uint32_t begin, const uint32_t end, const uint32_t stride) const =0;
    virtual void accept_visitor(PlaneVisitor &v) =0;
    // access pixel by zoomlevel coordinate
    uint32_t zoom_rowpixelsize(int zoomlevel) const {
        return 1<<((zoomlevel+1)/2);
    }
    uint32_t zoom_colpixelsize(int zoomlevel) const {
        return 1<<((zoomlevel)/2);
    }
};

template <typename> class Plane;
class ConstantPlane;

struct PlaneVisitor {
    virtual void visit(Plane<ColorVal_intern_8>&) =0;
    virtual void visit(Plane<ColorVal_intern_16>&) =0;
    virtual void visit(Plane<ColorVal_intern_16u>&) =0;
    virtual void visit(Plane<ColorVal_intern_32>&) =0;
    virtual void visit(ConstantPlane&) =0;
    virtual ~PlaneVisitor() {}
};

#define SCALED(x) (((x-1)>>scale)+1)
#ifdef USE_SIMD
// pad to a multiple of 8
#define PAD(x) ((x) + 8-((x)%8))
#else
#define PAD(x) (x)
#endif
template <typename pixel_t> class Plane final : public GeneralPlane {
    std::valarray<pixel_t> data;
    const uint32_t width, height;
    int s;
public:
    Plane(uint32_t w, uint32_t h, ColorVal color=0, int scale = 0) : data(color, PAD(SCALED(w)*SCALED(h))), width(SCALED(w)), height(SCALED(h)), s(scale) { }
    void clear() {
        data.clear();
    }
    void set(const uint32_t r, const uint32_t c, const ColorVal x) {
        const uint32_t sr = r>>s, sc = c>>s;
        assert(sr<height); assert(sc<width);
        data[sr*width + sc] = x;
    }
    ColorVal get(const uint32_t r, const uint32_t c) const ATTRIBUTE_HOT {
//        if (r >= height || r < 0 || c >= width || c < 0) {printf("OUT OF RANGE!\n"); return 0;}
        const uint32_t sr = r>>s, sc = c>>s;
        assert(sr<height); assert(sc<width);
        return data[sr*width + sc];
    }
#ifdef USE_SIMD
    FourColorVals get4(const uint32_t pos) const ATTRIBUTE_HOT {
        FourColorVals x {data[pos], data[pos+1], data[pos+2], data[pos+3]};
        return x;
    }
    void set4(const uint32_t pos, const FourColorVals x) {
        data[pos]=x[0];
        data[pos+1]=x[1];
        data[pos+2]=x[2];
        data[pos+3]=x[3];
    }
    EightColorVals get8(const uint32_t pos) const ATTRIBUTE_HOT {
        EightColorVals x {(int16_t)data[pos], (int16_t)data[pos+1], (int16_t)data[pos+2], (int16_t)data[pos+3],
                          (int16_t)data[pos+4], (int16_t)data[pos+5], (int16_t)data[pos+6], (int16_t)data[pos+7]};
        return x;
    }
    void set8(const uint32_t pos, const EightColorVals x) {
        data[pos]=x[0];
        data[pos+1]=x[1];
        data[pos+2]=x[2];
        data[pos+3]=x[3];
        data[pos+4]=x[4];
        data[pos+5]=x[5];
        data[pos+6]=x[6];
        data[pos+7]=x[7];
    }
#endif
    void set(const int z, const uint32_t r, const uint32_t c, const ColorVal x) {
        set(r*zoom_rowpixelsize(z),c*zoom_colpixelsize(z),x);
    }
    ColorVal get(const int z, const uint32_t r, const uint32_t c) const {
        return get(r*zoom_rowpixelsize(z),c*zoom_colpixelsize(z));
    }
    void normalize_scale() { s = 0; }
    void check_equal(const ColorVal x, const uint32_t r, const uint32_t begin, const uint32_t end, const uint32_t stride) const{
        for(uint32_t c = begin; c < end; c+= stride) assert(x == get(r,c));
    }

    void accept_visitor(PlaneVisitor &v) {
        v.visit(*this);
    }
};

class ConstantPlane final : public GeneralPlane {
    ColorVal color;
public:
    ConstantPlane(ColorVal c) : color(c) {}
    void set(const uint32_t r, const uint32_t c, const ColorVal x) {
        assert(x == color);
    }
    ColorVal get(const uint32_t r, const uint32_t c) const {
        return color;
    }
#ifdef USE_SIMD
    FourColorVals get4(const uint32_t pos) const ATTRIBUTE_HOT {
        FourColorVals x {color,color,color,color};
        return x;
    }
    void set4(const uint32_t pos, const FourColorVals x) {
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
    void set8(const uint32_t pos, const EightColorVals x) {
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
    bool is_constant() const { return true; }

    void set(const int z, const uint32_t r, const uint32_t c, const ColorVal x) {
        assert(x == color);
    }
    ColorVal get(const int z, const uint32_t r, const uint32_t c) const {
        return color;
    }
    void check_equal(const ColorVal x, const uint32_t r, const uint32_t begin, const uint32_t end, const uint32_t stride) const{
        assert(x == color);
    }

    void accept_visitor(PlaneVisitor &v) {
        v.visit(*this);
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


class Image {
    std::unique_ptr<GeneralPlane> planes[5];
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
      palette = other.palette;
      frame_delay = other.frame_delay;
      col_begin = other.col_begin;
      col_end = other.col_end;
      seen_before = other.seen_before;
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
          for (uint32_t r=0; r<height; r++)
             for (uint32_t c=0; c<width; c++)
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

    Image(uint32_t width, uint32_t height, ColorVal min, ColorVal max, int planes, int s=0) : scale(s) {
        init(width, height, min, max, planes);
    }

    Image(int s=0) : scale(s) {
      width = height = 0;
      minval = maxval = 0;
      num = 0;
      frame_delay = 0;
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
      for (int p=0; p<num; p++) planes[p] = std::move(other.planes[p]);
      frame_delay = other.frame_delay;
#ifdef SUPPORT_HDR
      depth = other.depth;
      other.depth = 0;
#endif

      other.width = other.height = 0;
      other.minval = other.maxval = 0;
      other.num = 0;
      other.frame_delay = 0;

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

    bool init(uint32_t w, uint32_t h, ColorVal min, ColorVal max, int p) {
      width = w;
      height = h;
      minval = min;
      maxval = max;
      col_begin.clear();
      col_begin.resize(height,0);
      col_end.clear();
      col_end.resize(height,width);
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

      clear();
      try {
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
    bool load(const char *name);
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
    }
    void abort_decoding() {
        width = 0; // this is used to signal the decoder to stop
    }

};

typedef std::vector<Image>    Images;
