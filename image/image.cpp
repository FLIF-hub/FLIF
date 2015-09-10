#include <string.h>
#include <stdio.h>

#include "image.h"
#include "image-png.h"
#include "image-pnm.h"

void Plane::init(int subwidth, int subheight, ColorVal min, ColorVal max)
{
    this->subwidth = subwidth;
    this->subheight = subheight;
    this->min = min;
    this->max = max;
//    this->data = std::vector<ColorVal>(subwidth*subheight, 0);
//    this->data = std::valarray<ColorVal>((ColorVal)-2, subwidth*subheight);
    this->data = std::valarray<ColorVal_intern>(subwidth*subheight);
}

bool Image::load(const char *filename)
{
    const char *f = strrchr(filename,'/');
    const char *ext = f ? strrchr(f,'.') : strrchr(filename,'.');
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

void Image::add_plane(ColorVal min, ColorVal max, int subSampleR, int subSampleC)
{
    planes.push_back(Plane((width+subSampleR-1)/subSampleR, (height+subSampleC-1)/subSampleC, min, max));
    subsample.push_back(std::make_pair(subSampleR, subSampleC));
}

void Image::init(int width, int height, ColorVal min, ColorVal max, int planes)
{
    this->planes = std::vector<Plane>(planes, Plane(width, height, min, max));
    this->width = width;
    this->height = height;
    this->subsample = std::vector<std::pair<int,int> >(planes, std::make_pair(1,1));
}

