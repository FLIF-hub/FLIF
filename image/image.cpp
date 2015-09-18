#include <string.h>
#include <stdio.h>

#include "image.h"
#include "image-png.h"
#include "image-pnm.h"
#include "../flif.h"


bool Image::load(const char *filename)
{
    const char *f = strrchr(filename,'/');
    const char *ext = f ? strrchr(f,'.') : strrchr(filename,'.');
    v_printf(2,"Loading input file: %s\n",filename);
    if (ext && !strcasecmp(ext,".png")) {
        return !image_load_png(filename,*this);
    }
    if (ext && !strcasecmp(ext,".pnm")) {
        return image_load_pnm(filename,*this);
    }
    if (ext && !strcasecmp(ext,".pbm")) {
        return image_load_pnm(filename,*this);
    }
    if (ext && !strcasecmp(ext,".pgm")) {
        return image_load_pnm(filename,*this);
    }
    if (ext && !strcasecmp(ext,".ppm")) {
        return image_load_pnm(filename,*this);
    }
    if (image_load_pnm(filename,*this) || !image_load_png(filename,*this)) return true;
    fprintf(stderr,"ERROR: Unknown extension for read from: %s\n",ext ? ext : "(none)");
    return false;
}

bool Image::save(const char *filename) const
{
    const char *f = strrchr(filename,'/');
    const char *ext = f ? strrchr(f,'.') : strrchr(filename,'.');
    v_printf(2,"Saving output file: %s\n",filename);
    if (ext && !strcasecmp(ext,".png")) {
        return !image_save_png(filename,*this);
    }
    if (ext && !strcasecmp(ext,".pnm")) {
        return image_save_pnm(filename,*this);
    }
    if (ext && !strcasecmp(ext,".pgm")) {
        return image_save_pnm(filename,*this);
    }
    if (ext && !strcasecmp(ext,".ppm")) {
        return image_save_pnm(filename,*this);
    }
    fprintf(stderr,"ERROR: Unknown extension to write to: %s\n",ext ? ext : "(none)");
    return false;
}
bool Image::save(const char *filename, const int scale) const
{
    if (scale == 1) return this->save(filename);
    Image downscaled;
    downscaled.init(this->width/scale, this->height/scale, this->min(0), this->max(0), this->numPlanes());
    v_printf(3,"Downscaled output: %ix%i -> %ix%i\n",this->width,this->height,this->width/scale,this->height/scale);
    for (int p=0; p<downscaled.numPlanes(); p++) {
        for (int r=0; r<downscaled.rows(); r++) {
            for (int c=0; c<downscaled.cols(); c++) {
                    downscaled.set(p,r,c, this->operator()(p,r*scale,c*scale));
            }
        }
    }
    return downscaled.save(filename);
}

void Image::add_plane(ColorVal min, ColorVal max)
{
    planes.push_back(Plane<ColorVal_intern>(width, height, min, max));
}

void Image::init(int width, int height, ColorVal min, ColorVal max, int planes)
{
    this->planes = std::vector<Plane<ColorVal_intern> >(planes, Plane<ColorVal_intern>(width, height, min, max));
    this->width = width;
    this->height = height;
}

