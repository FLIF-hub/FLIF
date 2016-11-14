#include <string.h>

#include "image.hpp"
#include "image-png.hpp"
#include "image-pnm.hpp"
#include "image-pam.hpp"
#include "image-rggb.hpp"
#include "image-metadata.hpp"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

#ifdef HAS_ENCODER
bool Image::load(const char *filename, metadata_options &options)
{
    if (!strcmp(filename,"-")) {
        v_printf(2,"Reading input as PAM/PPM from standard input.  ");
        return image_load_pnm(filename,*this);
    }
    const char *f = strrchr(filename,'/');
    const char *ext = f ? strrchr(f,'.') : strrchr(filename,'.');
    v_printf(2,"Loading input file: %s  ",filename);
    if (ext && !strcasecmp(ext,".png")) {
        return !image_load_png(filename,*this,options);
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
        return image_load_rggb(filename,*this, options);
    }
    if (ext && !strcasecmp(ext,".icc")) {
        return image_load_metadata(filename,*this,"iCCP");
    }
    if (ext && !strcasecmp(ext,".xmp")) {
        return image_load_metadata(filename,*this,"eXmp");
    }
    if (ext && !strcasecmp(ext,".exif")) {
        return image_load_metadata(filename,*this,"eXif");
    }
    if (image_load_pnm(filename,*this) || !image_load_png(filename,*this,options)) return true;
    e_printf("ERROR: Unknown input file type to read from: %s\n",ext ? ext : "(none)");
    return false;
}
#endif

bool Image::save(const char *filename) const
{
    if (!strcmp(filename,"-")) {
        v_printf(2,"Writing output as PAM to standard output.  ");
        return image_save_pam(filename,*this);
    }
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
    if (ext && !strcasecmp(ext,".icc")) {
        return image_save_metadata(filename,*this,"iCCP");
    }
    if (ext && !strcasecmp(ext,".xmp")) {
        return image_save_metadata(filename,*this,"eXmp");
    }
    if (ext && !strcasecmp(ext,".exif")) {
        return image_save_metadata(filename,*this,"eXif");
    }
    e_printf("ERROR: Unknown extension to write to: %s\n",ext ? ext : "(none)");
    return false;
}
