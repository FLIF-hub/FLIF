#pragma once

#include <vector>
#include <assert.h>
#include <stdint.h>
#include <valarray>
#include <memory>
#include "crc32k.h"

#include "../io.h"

typedef int32_t ColorVal;  // used in computations

typedef int16_t ColorVal_intern_8;   // used in representations
typedef int32_t ColorVal_intern_16;  // making them signed and a large enough for big palettes and negative alpha values for frame combination
typedef int32_t ColorVal_intern_32;

// It's a part of C++14. Following impl was taken from GotW#102
// (http://herbsutter.com/gotw/_102/).
// It should go into some common header one day.
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args &&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template <typename pixel_t> class Plane {
protected:
    std::valarray<pixel_t> data;
public:
    const uint32_t width, height;
    Plane(uint32_t w, uint32_t h) : data(w*h), width(w), height(h) { }

    void set(const uint32_t r, const uint32_t c, const ColorVal x) {
        data[r*width + c] = x;
    }

    ColorVal get(const uint32_t r, const uint32_t c) const {
//        if (r >= height || r < 0 || c >= width || c < 0) {printf("OUT OF RANGE!\n"); return 0;}
        return data[r*width + c];
    }
};

class Image {
    Image(const Image& other) = delete;
    Image& operator=(const Image& other) = delete;

    template <typename CV>
    using PlanePtr = std::unique_ptr<Plane<CV>>;

    PlanePtr<ColorVal_intern_8> plane_8_1;
    PlanePtr<ColorVal_intern_8> plane_8_2;
    PlanePtr<ColorVal_intern_16> plane_16_1;
    PlanePtr<ColorVal_intern_16> plane_16_2;
    PlanePtr<ColorVal_intern_32> plane_32_1;
    PlanePtr<ColorVal_intern_32> plane_32_2;
    uint32_t width, height;
    ColorVal minval,maxval;
    int num;
#ifdef SUPPORT_HDR
    int depth;
#else
    const int depth=8;
#endif

public:
    bool palette;
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
#ifdef SUPPORT_HDR
      depth = 0;
#endif
      palette = false;
      seen_before = 0;
    }

    // move constructor
    Image(Image&& other) {
      // reuse implementation from assignment operator
      operator=(std::move(other));
    }
    Image& operator=(Image&& other) {
      plane_8_1 = std::move(other.plane_8_1);
      plane_8_2 = std::move(other.plane_8_2);
      plane_16_1 = std::move(other.plane_16_1);
      plane_16_2 = std::move(other.plane_16_2);
#ifdef SUPPORT_HDR
      plane_32_1 = std::move(other.plane_32_1);
      plane_32_2 = std::move(other.plane_32_2);
#endif

      width = other.width;
      height = other.height;
      minval = other.minval;
      maxval = other.maxval;
      num = other.num;
#ifdef SUPPORT_HDR
      depth = other.depth;
      other.depth = 0;
#endif

      other.width = other.height = 0;
      other.minval = other.maxval = 0;
      other.num = 0;

      palette = other.palette;
      col_begin = std::move(other.col_begin);
      col_end = std::move(other.col_end);
      seen_before = other.seen_before;

      other.palette = false;
      other.seen_before = 0;
      return *this;
    }

    void init(uint32_t w, uint32_t h, ColorVal min, ColorVal max, int p) {
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

      palette=false;
      assert(min == 0);
      assert(max < (1<<depth));
      assert(p <= 4);
      clear();
      if (depth <= 8) {
        if (p>0) plane_8_1 = make_unique<Plane<ColorVal_intern_8>>(width, height); // R,Y
        if (p>1) plane_16_1 = make_unique<Plane<ColorVal_intern_16>>(width, height); // G,I
        if (p>2) plane_16_2 = make_unique<Plane<ColorVal_intern_16>>(width, height); // B,Q
        if (p>3) plane_8_2 = make_unique<Plane<ColorVal_intern_8>>(width, height); // A
#ifdef SUPPORT_HDR
      } else {
        if (p>0) plane_16_1 = make_unique<Plane<ColorVal_intern_16>>(width, height); // R,Y
        if (p>1) plane_32_1 = make_unique<Plane<ColorVal_intern_32>>(width, height); // G,I
        if (p>2) plane_32_2 = make_unique<Plane<ColorVal_intern_32>>(width, height); // B,Q
        if (p>3) plane_16_2 = make_unique<Plane<ColorVal_intern_16>>(width, height); // A
#endif
      }
    }

    void clear() {
      plane_8_1.reset(nullptr);
      plane_8_2.reset(nullptr);
      plane_16_1.reset(nullptr);
      plane_16_2.reset(nullptr);
#ifdef SUPPORT_HDR
      plane_32_1.reset(nullptr);
      plane_32_2.reset(nullptr);
#endif
    }

    void reset() {
        clear();
        init(0,0,0,0,0);
    }
    bool uses_alpha() const {
        if (num<4) return false;
        for (uint32_t r=0; r<height; r++)
           for (uint32_t c=0; c<width; c++)
              if (operator()(3,r,c) < (1<<depth)-1) return true;
        return false; // alpha plane is completely opaque, so it is useless
    }
    void drop_alpha() {
        if (num<4) return;
        assert(num==4);
        if (depth <= 8) {
                plane_8_2.reset(nullptr);
        } else {
                plane_16_2.reset(nullptr);
        }
        num=3;
    }
    void ensure_alpha() {
        switch(num) {
            case 1:
              if (depth <= 8) {
                plane_16_1 = make_unique<Plane<ColorVal_intern_16>>(width, height); // G,I
                plane_16_2 = make_unique<Plane<ColorVal_intern_16>>(width, height); // B,Q
#ifdef SUPPORT_HDR
              } else {
                plane_32_1 = make_unique<Plane<ColorVal_intern_32>>(width, height); // G,I
                plane_32_2 = make_unique<Plane<ColorVal_intern_32>>(width, height); // B,Q
#endif
              }
              for (uint32_t r=0; r<height; r++) {
               for (uint32_t c=0; c<width; c++) {
//                 set(1,r,c, operator()(0,r,c));
//                 set(2,r,c, operator()(0,r,c));
                 set(1,r,c, (1<<depth)-1); // I=0
                 set(2,r,c, (1<<depth)-1); // Q=0
               }
              }
            case 3:
              if (depth <= 8) {
                plane_8_2 = make_unique<Plane<ColorVal_intern_8>>(width, height); // A
              } else {
                plane_16_2 = make_unique<Plane<ColorVal_intern_16>>(width, height); // A
              }
              num=4;
              for (uint32_t r=0; r<height; r++) {
               for (uint32_t c=0; c<width; c++) {
                 set(3,r,c, 1); //(1<<depth)-1);
               }
              }
            case 4: // nothing to be done, we already have an alpha channel
              break;
            default: e_printf("OOPS: ensure_alpha() problem");
        }
    }

    bool load(const char *name);
    bool save(const char *name) const;
    bool save(const char *name, const int scale) const;

    // access pixel by coordinate
    ColorVal operator()(const int p, const uint32_t r, const uint32_t c) const {
#ifdef SUPPORT_HDR
      if (depth <= 8) {
#endif
        switch(p) {
          case 0: return plane_8_1->get(r,c);
          case 1: return plane_16_1->get(r,c);
          case 2: return plane_16_2->get(r,c);
          default: return plane_8_2->get(r,c);
        }
#ifdef SUPPORT_HDR
      } else {
        switch(p) {
          case 0: return plane_16_1->get(r,c);
          case 1: return plane_32_1->get(r,c);
          case 2: return plane_32_2->get(r,c);
          default: return plane_16_2->get(r,c);
        }
      }
#endif
    }
    void set(int p, uint32_t r, uint32_t c, ColorVal x) {
#ifdef SUPPORT_HDR
      if (depth <= 8) {
#endif
        switch(p) {
          case 0: return plane_8_1->set(r,c,x);
          case 1: return plane_16_1->set(r,c,x);
          case 2: return plane_16_2->set(r,c,x);
          default: return plane_8_2->set(r,c,x);
        }
#ifdef SUPPORT_HDR
      } else {
        switch(p) {
          case 0: return plane_16_1->set(r,c,x);
          case 1: return plane_32_1->set(r,c,x);
          case 2: return plane_32_2->set(r,c,x);
          default: return plane_16_2->set(r,c,x);
        }
      }
#endif
    }

    int numPlanes() const {
        return num;
    }

    ColorVal min(int) const {
        return minval;
    }

    ColorVal max(int) const {
        return maxval;
    }

    uint32_t rows() const {
        return height;
    }

    uint32_t cols() const {
        return width;
    }

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
//        if (p==0) fprintf(stdout,"Reading pixel at zoomlevel %i, position %i,%i, actual position %i,%i\n",z,rz,cz,rz*zoom_rowpixelsize(z),cz*zoom_colpixelsize(z));
//        return operator()(p,rz*zoom_rowpixelsize(z),cz*zoom_colpixelsize(z));
        uint32_t r = rz*zoom_rowpixelsize(z);
        uint32_t c = cz*zoom_colpixelsize(z);
//        if (p==0 && r>= 0 && c>=0 && r<width &&c<height) fprintf(stdout,"Reading pixel at zoomlevel %i, position %i,%i, actual position %i,%i\n",z,rz,cz,rz*zoom_rowpixelsize(z),cz*zoom_colpixelsize(z));
        return operator()(p,r,c);
    }
//    ColorVal_intern& operator()(int p, int z, int rz, int cz) {
//        int r = rz*zoom_rowpixelsize(z);
//        int c = cz*zoom_colpixelsize(z);
//        return planes[p](r,c);
//    }
    void set(int p, int z, uint32_t rz, uint32_t cz, ColorVal x) {
//        return operator()(p,rz*zoom_rowpixelsize(z),cz*zoom_colpixelsize(z));
        uint32_t r = rz*zoom_rowpixelsize(z);
        uint32_t c = cz*zoom_colpixelsize(z);
//        if (p==0 && r>= 0 && c>=0 && r<width &&c<height) fprintf(stdout,"Writing to pixel at zoomlevel %i, position %i,%i, actual position %i,%i\n",z,rz,cz,rz*zoom_rowpixelsize(z),cz*zoom_colpixelsize(z));
        set(p,r,c,x);
    }

    uint32_t checksum() {
          uint_fast32_t crc=0;
          crc32k_transform(crc,width & 255);
          crc32k_transform(crc,width / 256);
          crc32k_transform(crc,height & 255);
          crc32k_transform(crc,height / 256);
          for (int p=0; p<num; p++) {
//            for (ColorVal d : p.data) {
            for (uint32_t r=0; r<height; r++) {
               for (uint32_t c=0; c<width; c++) {
                 ColorVal d = operator()(p,r,c);
                 crc32k_transform(crc,d & 255);
                 crc32k_transform(crc,d / 256);
              }
            }
          }
//          printf("Computed checksum: %X\n", (~crc & 0xFFFFFFFF));
          return (~crc & 0xFFFFFFFF);
    }

};

typedef std::vector<Image>    Images;
