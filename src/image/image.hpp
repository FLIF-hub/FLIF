#pragma once

#include <vector>
#include <assert.h>
#include <stdint.h>
#include <valarray>
#include <memory>
#include "crc32k.hpp"

#include "../io.hpp"
#include "../config.h"

typedef int32_t ColorVal;  // used in computations

typedef uint8_t ColorVal_intern_8;
typedef int16_t ColorVal_intern_16;
typedef int32_t ColorVal_intern_32;

// It's a part of C++14. Following impl was taken from GotW#102
// (http://herbsutter.com/gotw/_102/).
// It should go into some common header one day.
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args &&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

class GeneralPlane {
public:
    virtual void set(const uint32_t r, const uint32_t c, const ColorVal x) =0;
    virtual ColorVal get(const uint32_t r, const uint32_t c) const =0;
    virtual bool is_constant() const { return false; }
};


template <typename pixel_t> class Plane : public GeneralPlane {
public:
    std::valarray<pixel_t> data;
    const uint32_t width, height;
    Plane(uint32_t w, uint32_t h, ColorVal color=0) : data(color, w*h), width(w), height(h) { }
    void set(const uint32_t r, const uint32_t c, const ColorVal x) {
        data[r*width + c] = x;
    }
    ColorVal get(const uint32_t r, const uint32_t c) const {
//        if (r >= height || r < 0 || c >= width || c < 0) {printf("OUT OF RANGE!\n"); return 0;}
        return data[r*width + c];
    }
};

class ConstantPlane : public GeneralPlane {
public:
    ColorVal color;
    ConstantPlane(ColorVal c) : color(c) {}
    void set(const uint32_t r, const uint32_t c, const ColorVal x) {
        assert(x == color);
    }
    ColorVal get(const uint32_t r, const uint32_t c) const {
        return color;
    }
    bool is_constant() const { return true; }
};

class Image {
    std::unique_ptr<GeneralPlane> planes[5];
    uint32_t width, height;
    ColorVal minval,maxval;
    int num;
#ifdef SUPPORT_HDR
    int depth;
#else
    const int depth=8;
#endif

    Image(const Image& other)
    {
      // reuse implementation from assignment operator
      operator=(other);
    }

    Image& operator=(const Image& other) {
      width = other.width;
      height = other.height;
      minval = other.minval;
      maxval = other.maxval;
      num = other.num;
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
        if (p>0) planes[0] = make_unique<Plane<ColorVal_intern_8>>(width, height); // R,Y
        if (p>1) planes[1] = make_unique<Plane<ColorVal_intern_16>>(width, height); // G,I
        if (p>2) planes[2] = make_unique<Plane<ColorVal_intern_16>>(width, height); // B,Q
        if (p>3) planes[3] = make_unique<Plane<ColorVal_intern_8>>(width, height); // A
#ifdef SUPPORT_HDR
      } else {
        if (p>0) planes[0] = make_unique<Plane<ColorVal_intern_16>>(width, height); // R,Y
        if (p>1) planes[1] = make_unique<Plane<ColorVal_intern_32>>(width, height); // G,I
        if (p>2) planes[2] = make_unique<Plane<ColorVal_intern_32>>(width, height); // B,Q
        if (p>3) planes[3] = make_unique<Plane<ColorVal_intern_16>>(width, height); // A
#endif
      }
      if (p>4) planes[4] = make_unique<Plane<ColorVal_intern_8>>(width, height); // FRA
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

    Image(uint32_t width, uint32_t height, ColorVal min, ColorVal max, int planes) {
        init(width, height, min, max, planes);
    }

    Image() {
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
        if (p>0) planes[0] = make_unique<Plane<ColorVal_intern_8>>(width, height); // R,Y
        if (p>1) planes[1] = make_unique<Plane<ColorVal_intern_16>>(width, height); // G,I
        if (p>2) planes[2] = make_unique<Plane<ColorVal_intern_16>>(width, height); // B,Q
        if (p>3) planes[3] = make_unique<Plane<ColorVal_intern_8>>(width, height); // A
#ifdef SUPPORT_HDR
      } else {
        if (p>0) planes[0] = make_unique<Plane<ColorVal_intern_16>>(width, height); // R,Y
        if (p>1) planes[1] = make_unique<Plane<ColorVal_intern_32>>(width, height); // G,I
        if (p>2) planes[2] = make_unique<Plane<ColorVal_intern_32>>(width, height); // B,Q
        if (p>3) planes[3] = make_unique<Plane<ColorVal_intern_16>>(width, height); // A
#endif
      }
      if (p>4) planes[4] = make_unique<Plane<ColorVal_intern_8>>(width, height); // FRA
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
        if (p==0) planes[0] = make_unique<Plane<ColorVal_intern_8>>(width, height, val); // R,Y
        if (p==1) planes[1] = make_unique<Plane<ColorVal_intern_16>>(width, height, val); // G,I
        if (p==2) planes[2] = make_unique<Plane<ColorVal_intern_16>>(width, height, val); // B,Q
        if (p==3) planes[3] = make_unique<Plane<ColorVal_intern_8>>(width, height, val); // A
#ifdef SUPPORT_HDR
      } else {
        if (p==0) planes[0] = make_unique<Plane<ColorVal_intern_16>>(width, height, val); // R,Y
        if (p==1) planes[1] = make_unique<Plane<ColorVal_intern_32>>(width, height, val); // G,I
        if (p==2) planes[2] = make_unique<Plane<ColorVal_intern_32>>(width, height, val); // B,Q
        if (p==3) planes[3] = make_unique<Plane<ColorVal_intern_16>>(width, height, val); // A
#endif
      }
    }
    void ensure_frame_lookbacks() {
        switch(num) {
            case 1:
              num=3;
              make_constant_plane(1,((1<<depth)-1));
              make_constant_plane(2,((1<<depth)-1));
            case 3:
              make_constant_plane(3,255);
            case 4:
              planes[4] = make_unique<Plane<ColorVal_intern_8>>(width, height);
              num=5;
            default:
              assert(num==5);
              num=5;
        }
    }

#ifdef HAS_ENCODER
    bool load(const char *name);
#endif
    bool save(const char *name) const;
    bool save(const char *name, const int scale) const;

    // access pixel by coordinate
    ColorVal operator()(const int p, const uint32_t r, const uint32_t c) const {
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
    ColorVal operator()(int p, int z, uint32_t rz, uint32_t cz) const {
        uint32_t r = rz*zoom_rowpixelsize(z);
        uint32_t c = cz*zoom_colpixelsize(z);
        return operator()(p,r,c);
    }
    void set(int p, int z, uint32_t rz, uint32_t cz, ColorVal x) {
        uint32_t r = rz*zoom_rowpixelsize(z);
        uint32_t c = cz*zoom_colpixelsize(z);
        set(p,r,c,x);
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
                  crc32k_transform(crc, d >> 8);
               }
//          printf("Computed checksum: %X\n", (~crc & 0xFFFFFFFF));
          return (~crc & 0xFFFFFFFF);
    }

};

typedef std::vector<Image>    Images;
