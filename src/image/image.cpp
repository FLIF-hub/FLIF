#include <string.h>

#include "image.hpp"
#include "image-png.hpp"
#include "image-pnm.hpp"
#include "image-pam.hpp"
#include "image-rggb.hpp"

#ifdef _MSC_VER
#define strcasecmp stricmp
#endif

#ifdef HAS_ENCODER
bool Image::load(const char *filename)
{
    const char *f = strrchr(filename,'/');
    const char *ext = f ? strrchr(f,'.') : strrchr(filename,'.');
    v_printf(2,"Loading input file: %s  ",filename);
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
    if (ext && !strcasecmp(ext,".pam")) {
        return image_load_pam(filename,*this);
    }
    if (ext && !strcasecmp(ext,".rggb")) {
        return image_load_rggb(filename,*this);
    }
    if (image_load_pnm(filename,*this) || !image_load_png(filename,*this)) return true;
    e_printf("ERROR: Unknown input file type to read from: %s\n",ext ? ext : "(none)");
    return false;
}
#endif

bool Image::save(const char *filename) const
{
    const char *f = strrchr(filename,'/');
    const char *ext = f ? strrchr(f,'.') : strrchr(filename,'.');
    v_printf(2,"Saving output file: %s  ",filename);
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
    if (ext && !strcasecmp(ext,".pam")) {
        return image_save_pam(filename,*this);
    }
    if (ext && !strcasecmp(ext,".rggb")) {
        return image_save_rggb(filename,*this);
    }
    e_printf("ERROR: Unknown extension to write to: %s\n",ext ? ext : "(none)");
    return false;
}
