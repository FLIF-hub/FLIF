#ifndef _IMAGE_H_
#define _IMAGE_H_ 1

#include <vector>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <valarray>
#include "crc32k.h"

typedef int32_t ColorVal;  // used in computations
//typedef uint8_t ColorVal_intern; // used in representations
typedef int16_t ColorVal_intern; // used in representations


template <typename pixel_t> class Plane {
public:
    std::valarray<pixel_t> data;
    int width, height;
    ColorVal min, max;

    Plane(int w, int h, ColorVal mi, ColorVal ma) : data(w*h), width(w), height(h), min(mi), max(ma) { }

    Plane() { }
//        init(0,0,0,0);
//    }

//    void init(int w, int h, ColorVal mi, ColorVal ma) : data(w*h), width(w), height(h), min(min), max(ma) { }

//    ColorVal_intern &operator()(int r, int c) {
//        assert(!(r >= height || r < 0 || c >= width || c < 0));
//        return data[r*width + c];
//    }
    void set(int r, int c, ColorVal x) {
        data[r*width + c] = x;
    }

    ColorVal operator()(int r, int c) const {
//        if (r >= height || r < 0 || c >= width || c < 0) {printf("OUT OF RANGE!\n"); return 0;}
        return data[r*width + c];
    }
};

class Image {
protected:
    int width, height;
    std::vector<Plane<ColorVal_intern> > planes;

public:
    Plane<ColorVal_intern> &operator()(int plane) {
        return planes[plane];
    }
    const Plane<ColorVal_intern> &operator()(int plane) const {
        return planes[plane];
    }

    Image(int width, int height, ColorVal min, ColorVal max, int planes) {
        init(width, height, min, max, planes);
    }

    Image() {
        reset();
    }

    void init(int width, int height, ColorVal min, ColorVal max, int planes);

    void reset() {
        init(0,0,0,0,0);
    }

    void add_plane(ColorVal min, ColorVal max);

    void drop_planes(int newsize) {
        planes.resize(newsize);
    }

    bool load(const char *name);
    bool save(const char *name) const;
    bool save(const char *name, const int scale) const;

    // access pixel by coordinate
    ColorVal operator()(int p, int r, int c) const {
        return planes[p](r,c);
    }
//    ColorVal_intern& operator()(int p, int r, int c) {
//        return planes[p](r,c);
//    }
    void set(int p, int r, int c, ColorVal x) {
        planes[p].set(r,c,x);
    }

    int numPlanes() const {
        return planes.size();
    }

    int min(int p) const {
        return planes[p].min;
    }

    int max(int p) const {
        return planes[p].max;
    }

    int rows() const {
        return height;
    }

    int cols() const {
        return width;
    }

    // access pixel by zoomlevel coordinate
    int zoom_rowpixelsize(int zoomlevel) const {
        return 1<<((zoomlevel+1)/2);
    }
    int zoom_colpixelsize(int zoomlevel) const {
        return 1<<((zoomlevel)/2);
    }

    int rows(int zoomlevel) const {
        return 1+(rows()-1)/zoom_rowpixelsize(zoomlevel);
    }
    int cols(int zoomlevel) const {
        return 1+(cols()-1)/zoom_colpixelsize(zoomlevel);
    }
    int zooms() const {
        int z = 0;
        while (zoom_rowpixelsize(z) < rows() || zoom_colpixelsize(z) < cols()) z++;
        return z;
    }
    ColorVal operator()(int p, int z, int rz, int cz) const {
//        if (p==0) fprintf(stdout,"Reading pixel at zoomlevel %i, position %i,%i, actual position %i,%i\n",z,rz,cz,rz*zoom_rowpixelsize(z),cz*zoom_colpixelsize(z));
//        return operator()(p,rz*zoom_rowpixelsize(z),cz*zoom_colpixelsize(z));
        int r = rz*zoom_rowpixelsize(z);
        int c = cz*zoom_colpixelsize(z);
//        if (p==0 && r>= 0 && c>=0 && r<width &&c<height) fprintf(stdout,"Reading pixel at zoomlevel %i, position %i,%i, actual position %i,%i\n",z,rz,cz,rz*zoom_rowpixelsize(z),cz*zoom_colpixelsize(z));
        return planes[p](r,c);
    }
//    ColorVal_intern& operator()(int p, int z, int rz, int cz) {
//        int r = rz*zoom_rowpixelsize(z);
//        int c = cz*zoom_colpixelsize(z);
//        return planes[p](r,c);
//    }
    void set(int p, int z, int rz, int cz, ColorVal x) {
//        return operator()(p,rz*zoom_rowpixelsize(z),cz*zoom_colpixelsize(z));
        int r = rz*zoom_rowpixelsize(z);
        int c = cz*zoom_colpixelsize(z);
//        if (p==0 && r>= 0 && c>=0 && r<width &&c<height) fprintf(stdout,"Writing to pixel at zoomlevel %i, position %i,%i, actual position %i,%i\n",z,rz,cz,rz*zoom_rowpixelsize(z),cz*zoom_colpixelsize(z));
        return planes[p].set(r,c,x);
    }

    uint32_t checksum() {
          uint_fast32_t crc=0;
          crc32k_transform(crc,width & 255);
          crc32k_transform(crc,width / 256);
          crc32k_transform(crc,height & 255);
          crc32k_transform(crc,height / 256);
          for (auto p : planes) {
            for (ColorVal d : p.data) {
                crc32k_transform(crc,d & 255);
                crc32k_transform(crc,d / 256);
            }
          }
          return (~crc & 0xFFFFFFFF);
    }

};

#endif
