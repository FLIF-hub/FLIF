#ifndef _IMAGE_H_
#define _IMAGE_H_ 1

#include <vector>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <valarray>
#include "../flif_config.h"
#include "crc32k.h"

typedef int16_t ColorVal;  // used in computations
//typedef uint8_t ColorVal_intern; // used in representations
typedef int16_t ColorVal_intern; // used in representations


class Plane
{
public:
    int subwidth, subheight;
    ColorVal min, max;
    std::valarray<ColorVal_intern> data;

    Plane(int subwidth, int subheight, ColorVal min, ColorVal max) {
        init(subwidth, subheight, min, max);
    }

    Plane() {
        init(0,0,0,0);
    }

    void init(int subwidth, int subheight, ColorVal min, ColorVal max);

    ColorVal_intern &operator()(int subr, int subc) {
        assert(!(subr >= subheight || subr < 0 || subc >= subwidth || subc < 0));
        return data[subr*subwidth + subc];
    }

    ColorVal operator()(int subr, int subc) const {
//        if (subr >= subheight || subr < 0 || subc >= subwidth || subc < 0) {printf("OUT OF RANGE!\n"); return 0;}
        return data[subr*subwidth + subc];
    }
};

class Image
{
protected:
    int width, height;
    std::vector<Plane> planes;
    std::vector<std::pair<int,int> > subsample; // first=rows, seconds=cols


public:
    Plane &operator()(int plane) {
        return planes[plane];
    }
    const Plane &operator()(int plane) const {
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

    void add_plane(ColorVal min, ColorVal max, int subSampleR = 1, int subSampleC = 1);

    void drop_planes(int newsize) {
        planes.resize(newsize);
        subsample.resize(newsize);
    }

    bool load(const char *name);
    bool save(const char *name) const;

    bool is_set(int p, int r, int c) const {
        return ((r % subsample[p].first) == 0 && (c % subsample[p].second) == 0);
    }

    // access pixel by coordinate
    ColorVal operator()(int p, int r, int c) const {
        return planes[p](r,c);
//        return planes[p](r / subsample[p].first,c / subsample[p].second);
    }
    ColorVal_intern& operator()(int p, int r, int c) {
        return planes[p](r,c);
//        return planes[p](r / subsample[p].first,c / subsample[p].second);
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
    ColorVal_intern& operator()(int p, int z, int rz, int cz) {
//        return operator()(p,rz*zoom_rowpixelsize(z),cz*zoom_colpixelsize(z));
        int r = rz*zoom_rowpixelsize(z);
        int c = cz*zoom_colpixelsize(z);
//        if (p==0 && r>= 0 && c>=0 && r<width &&c<height) fprintf(stdout,"Writing to pixel at zoomlevel %i, position %i,%i, actual position %i,%i\n",z,rz,cz,rz*zoom_rowpixelsize(z),cz*zoom_colpixelsize(z));
        return planes[p](r,c);
    }

    int subSampleR(int p) const {
        return subsample[p].first;
    }

    int subSampleC(int p) const {
        return subsample[p].second;
    }
    void permute_planes(const std::vector<int> &p) {
        std::vector<Plane> oldplanes = planes;
        for (unsigned int i=0; i<p.size(); i++)
                planes[i] = oldplanes[p[i]];
    }
    uint32_t checksum() {
          uint_fast32_t crc=0;
          crc32k_transform(crc,width & 255);
          crc32k_transform(crc,width / 256);
          crc32k_transform(crc,height & 255);
          crc32k_transform(crc,height / 256);
          for (Plane p : planes) {
            for (ColorVal d : p.data) {
                crc32k_transform(crc,d & 255);
                crc32k_transform(crc,d / 256);
            }
          }
          return (~crc & 0xFFFFFFFF);
    }

};

#endif
