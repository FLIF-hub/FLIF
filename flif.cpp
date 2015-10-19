/*  FLIF - Free Lossless Image Format
 Copyright (C) 2010-2015  Jon Sneyers & Pieter Wuille

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 Parts of this code are based on code from the FFMPEG project, in
 particular parts:
 - ffv1.c - Copyright (c) 2003 Michael Niedermayer <michaelni@gmx.at>
 - common.h - copyright (c) 2006 Michael Niedermayer <michaelni@gmx.at>
 - rangecoder.c - Copyright (c) 2004 Michael Niedermayer <michaelni@gmx.at>
*/

#include <string>
#include <string.h>

#include "maniac/rac.h"
#include "maniac/compound.h"
#include "maniac/util.h"

#include "image/color_range.h"
#include "transform/factory.h"

#include "flif_config.h"

#ifdef _MSC_VER
#include "getopt/getopt.h"
#else
#include <getopt.h>
#endif

#include <stdarg.h>

#include "common.h"
#ifdef HAS_ENCODER
#include "flif-enc.h"
#endif
#include "flif-dec.h"

#ifdef _MSC_VER
#define strcasecmp stricmp
#endif





// planes:
// 0    Y channel (luminance)
// 1    I (chroma)
// 2    Q (chroma)
// 3    Alpha (transparency)


/******************************************/
/*   scanlines encoding/decoding          */
/******************************************/



/******************************************/
/*   FLIF2 encoding/decoding              */
/******************************************/
void show_help() {
    printf("Usage:\n");
#ifdef HAS_ENCODER
    printf("   flif [encode options] <input image(s)> <output.flif>\n");
#endif
    printf("   flif [-d] [decode options] <input.flif> <output.pnm | output.pam | output.png>\n");
#ifdef HAS_ENCODER
    printf("   flif [-t] [decode options] [encode options] <input.flif> <output.flif>\n");
#endif
    printf("General Options:\n");
    printf("   -h, --help           show help\n");
    printf("   -v, --verbose        increase verbosity (multiple -v for more output)\n");
#ifdef HAS_ENCODER
    printf("Encode options:\n");
    printf("   -i, --interlace      interlacing (default, except for tiny images)\n");
    printf("   -n, --no-interlace   force no interlacing\n");
    printf("   -a, --acb            force auto color buckets (ACB)\n");
    printf("   -b, --no-acb         force no auto color buckets\n");
    printf("   -p, --palette=P      max palette size=P (default: P=512)\n");
    printf("   -r, --repeats=N      N repeats for MANIAC learning (default: N=%i)\n",TREE_LEARN_REPEATS);
    printf("   Input images should be PNG, PNM (PPM,PGM,PBM) or PAM files.\n");
    printf("   Multiple input images (for animated FLIF) must have the same dimensions.\n");
    printf("   -f, --frame-delay=D  delay between animation frames, in ms (default: D=100)\n");
    printf("   -l, --lookback=L     max lookback between frames (default: L=1)\n");
#endif
    printf("Decode options:\n");
    printf("   -q, --quality=Q      lossy decode quality at Q percent (0..100)\n");
    printf("   -s, --scale=S        lossy downscaled image at scale 1:S (2,4,8,16)\n");
}

bool file_exists(const char * filename){
        FILE * file = fopen(filename, "rb");
        if (!file) return false;
        fclose(file);
        return true;
}
bool file_is_flif(const char * filename){
        FILE * file = fopen(filename, "rb");
        if (!file) return false;
        char buff[5];
        bool result=true;
        if (!fgets(buff,5,file)) result=false;
        else if (strcmp(buff,"FLIF") && strcmp(buff,"!<ar")) result=false;  // assuming that it might be a FLIF file if it looks like an ar
        fclose(file);
        return result;
}

void show_banner() {
    v_printf(3,"  _____  __  (__) _____");
    v_printf(3,"\n (___  ||  | |  ||  ___)   ");v_printf(2,"FLIF 0.1 [19 October 2015]");
    v_printf(3,"\n  (__  ||  |_|__||  __)    Free Lossless Image Format");
    v_printf(3,"\n    (__||______) |__)    ");v_printf(2,"  (c) 2010-2015 J.Sneyers & P.Wuille, GNU GPL v3+\n");
    v_printf(3,"\n");
}

bool check_compatible_extension (char *ext) {
    if (!(ext && ( !strcasecmp(ext,".png") // easy to add or remove formats
                  || !strcasecmp(ext,".pnm")
                  || !strcasecmp(ext,".ppm")
                  || !strcasecmp(ext,".pgm")
                  || !strcasecmp(ext,".pbm")
                  || !strcasecmp(ext,".pam")
                  || !strcasecmp(ext,".rggb")))) {
        return false;
    } else {
        return true;
    }
}

#ifdef HAS_ENCODER

union flifEncodingOptional {
  flifEncoding encoding;
  Optional o;
  flifEncodingOptional() : o(Optional::undefined) {}
};


bool encode_load_input_images(int argc, char **argv, Images &images) {
    int nb_input_images = argc-1;
    while(argc>1) {
        Image image;
        v_printf(2,"\r");
        if (!image.load(argv[0])) {
            e_printf("Could not read input file: %s\n", argv[0]);
            return false;
        };
        images.push_back(std::move(image));
        const Image& last_image = images.back();
        if (last_image.rows() != images[0].rows() || last_image.cols() != images[0].cols() || last_image.numPlanes() != images[0].numPlanes()) {
            e_printf("Dimensions of all input images should be the same!\n");
            e_printf("  First image is %ux%u, %i channels.\n",images[0].cols(),images[0].rows(),images[0].numPlanes());
            e_printf("  This image is %ux%u, %i channels: %s\n",last_image.cols(),last_image.rows(),last_image.numPlanes(),argv[0]);
            return false;
        }
        argc--; argv++;
        if (nb_input_images>1) {v_printf(2,"    (%i/%i)         ",(int)images.size(),nb_input_images); v_printf(4,"\n");}
    }
    v_printf(2,"\n");
    return true;
}
bool encode_flif(int argc, char **argv, Images &images, int palette_size, int acb, flifEncodingOptional method, int lookback, int learn_repeats, int frame_delay) {
    bool flat=true;
    for (Image &image : images) if (image.uses_alpha()) flat=false;
    if (flat && images[0].numPlanes() == 4) {
        v_printf(2,"Alpha channel not actually used, dropping it.\n");
        for (Image &image : images) image.drop_alpha();
    }
    uint64_t nb_pixels = (uint64_t)images[0].rows() * images[0].cols();
    std::vector<std::string> desc;
    desc.push_back("YIQ");  // convert RGB(A) to YIQ(A)
    desc.push_back("BND");  // get the bounds of the color spaces
    if (palette_size > 0)
        desc.push_back("PLA");  // try palette (including alpha)
    if (palette_size > 0)
        desc.push_back("PLT");  // try palette (without alpha)
    if (acb == -1) {
        // not specified if ACB should be used
        if (nb_pixels > 10000) desc.push_back("ACB");  // try auto color buckets on large images
    } else if (acb) desc.push_back("ACB");  // try auto color buckets if forced
    if (method.o == Optional::undefined) {
        // no method specified, pick one heuristically
        if (nb_pixels < 10000) method.encoding=flifEncoding::nonInterlaced; // if the image is small, not much point in doing interlacing
        else method.encoding=flifEncoding::interlaced; // default method: interlacing
    }
    if (images.size() > 1) {
        desc.push_back("DUP");  // find duplicate frames
        desc.push_back("FRS");  // get the shapes of the frames
        if (lookback != 0) desc.push_back("FRA");  // make a "deep" alpha channel (negative values are transparent to some previous frame)
    }
    if (learn_repeats < 0) {
        // no number of repeats specified, pick a number heuristically
        learn_repeats = TREE_LEARN_REPEATS;
        if (nb_pixels < 5000) learn_repeats--;        // avoid large trees for small images
        if (learn_repeats < 0) learn_repeats=0;
    }
    FILE *file = fopen(argv[0],"wb");
    if (!file)
        return false;
    FileIO fio(file, argv[0]);
    return flif_encode(fio, images, desc, method.encoding, learn_repeats, acb, frame_delay, palette_size, lookback);
}

bool handle_encode(int argc, char **argv, Images &images, int palette_size, int acb, flifEncodingOptional method, int lookback, int learn_repeats, int frame_delay) {
    if (!encode_load_input_images(argc,argv,images)) return false;
    argv += (argc-1);
    argc = 1;
    return encode_flif(argc, argv, images, palette_size, acb, method, lookback, learn_repeats, frame_delay);
}
#endif

bool decode_flif(char **argv, Images &images, int quality, int scale) {
    FILE *file = fopen(argv[0],"rb");
    if(!file) return false;
    FileIO fio(file, argv[0]);
    return flif_decode(fio, images, quality, scale);
}

int handle_decode(char **argv, Images &images, int quality, int scale) {
    char *ext = strrchr(argv[1],'.');
    if (!check_compatible_extension(ext)) {
        e_printf("Error: expected \".png\", \".pnm\" or \".pam\" file name extension for output file\n");
        return 1;
    }
    if (!decode_flif(argv, images, quality, scale)) {e_printf("Error: could not decode FLIF file\n"); return 3; }
    if (scale>1)
        v_printf(3,"Downscaling output: %ux%u -> %ux%u\n",images[0].cols(),images[0].rows(),images[0].cols()/scale,images[0].rows()/scale);
    if (images.size() == 1) {
        if (!images[0].save(argv[1],scale)) return 2;
    } else {
        int counter=0;
        std::vector<char> vfilename(strlen(argv[1])+6);
        char *filename = &vfilename[0];
        strcpy(filename,argv[1]);
        char *a_ext = strrchr(filename,'.');
        for (Image& image : images) {
            sprintf(a_ext,"-%03d%s",counter++,ext);
            if (!image.save(filename,scale)) return 2;
            v_printf(2,"    (%i/%i)         \r",counter,(int)images.size()); v_printf(4,"\n");
        }
    }
    v_printf(2,"\n");
    return 0;
}
int main(int argc, char **argv)
{
    Images images;
#ifdef HAS_ENCODER
    int mode = 0; // 0 = encode, 1 = decode, 2 = transcode
    flifEncodingOptional method;
    int learn_repeats = -1;
    int acb = -1; // try auto color buckets
    int frame_delay = 100;
    int palette_size = 512;
    int lookback = 1;
#else
    int mode = 1;
#endif
    int quality = 100; // 100 = everything, positive value: partial decode, negative value: only rough data
    int scale = 1;
    if (strcmp(argv[0],"flif") == 0) mode = 0;
    if (strcmp(argv[0],"dflif") == 0) mode = 1;
    if (strcmp(argv[0],"deflif") == 0) mode = 1;
    if (strcmp(argv[0],"decflif") == 0) mode = 1;
    static struct option optlist[] = {
        {"help", 0, NULL, 'h'},
        {"decode", 0, NULL, 'd'},
        {"verbose", 0, NULL, 'v'},
        {"quality", 1, NULL, 'q'},
        {"scale", 1, NULL, 's'},
#ifdef HAS_ENCODER
        {"encode", 0, NULL, 'e'},
        {"transcode", 0, NULL, 't'},
        {"interlace", 0, NULL, 'i'},
        {"no-interlace", 0, NULL, 'n'},
        {"acb", 0, NULL, 'a'},
        {"no-acb", 0, NULL, 'b'},
        {"palette", 1, NULL, 'p'},
        {"repeats", 1, NULL, 'r'},
        {"frame-delay", 1, NULL, 'f'},
        {"lookback", 1, NULL, 'l'},
#endif
        {0, 0, 0, 0}
    };
    int i,c;
#ifdef HAS_ENCODER
    while ((c = getopt_long (argc, argv, "hedtvinabq:s:p:r:f:l:", optlist, &i)) != -1) {
#else
    while ((c = getopt_long (argc, argv, "hdvq:s:", optlist, &i)) != -1) {
#endif
        switch (c) {
        case 'd': mode=1; break;
        case 'v': increase_verbosity(); break;
        case 'q': quality=atoi(optarg);
                  if (quality < 0 || quality > 100) {e_printf("Not a sensible number for option -q\n"); return 1; }
                  break;
        case 's': scale=atoi(optarg);
                  if (scale < 1 || scale > 128) {e_printf("Not a sensible number for option -s\n"); return 1; }
                  break;
#ifdef HAS_ENCODER
        case 'e': mode=0; break;
        case 't': mode=2; break;
        case 'i': method.encoding=flifEncoding::interlaced; break;
        case 'n': method.encoding=flifEncoding::nonInterlaced; break;
        case 'a': acb=1; break;
        case 'b': acb=0; break;
        case 'p': palette_size=atoi(optarg);
                  if (palette_size < -1 || palette_size > 30000) {e_printf("Not a sensible number for option -p\n"); return 1; }
                  if (palette_size == 0) {v_printf(2,"Palette disabled\n"); }
                  break;
        case 'r': learn_repeats=atoi(optarg);
                  if (learn_repeats < 0 || learn_repeats > 1000) {e_printf("Not a sensible number for option -r\n"); return 1; }
                  break;
        case 'f': frame_delay=atoi(optarg);
                  if (frame_delay < 0 || frame_delay > 60000) {e_printf("Not a sensible number for option -f\n"); return 1; }
                  break;
        case 'l': lookback=atoi(optarg);
                  if (lookback < -1 || lookback > 256) {e_printf("Not a sensible number for option -l\n"); return 1; }
                  break;
#endif
        case 'h':
        default: show_help(); return 0;
        }
    }
    argc -= optind;
    argv += optind;

    show_banner();
    if (argc == 0) {
        //e_printf("Input file missing.\n");
        if (get_verbosity() == 1) show_help();
        return 1;
    }

    if (argc == 1) {
        show_help();
        e_printf("\nOutput file missing.\n");
        return 1;
    }

    if (file_exists(argv[0])) {
#ifdef HAS_ENCODER
        if (mode == 0 && file_is_flif(argv[0])) {
            char *f = strrchr(argv[1],'/');
            char *ext = f ? strrchr(f,'.') : strrchr(argv[1],'.');
            if (check_compatible_extension(ext)) {
                v_printf(2,"Input file is a FLIF file, adding implicit -d\n");
                mode = 1;
            } else if ((ext && ( !strcasecmp(ext,".flif")  || ( !strcasecmp(ext,".flf") )))) {
                v_printf(2,"Input and output file are both FLIF file, adding implicit -t\n");
                mode = 2;
            }
        }
#endif
        char *f = strrchr(argv[0],'/');
        char *ext = f ? strrchr(f,'.') : strrchr(argv[0],'.');
        if (mode == 0) {
            if (!check_compatible_extension(ext)) {
                e_printf("Warning: expected \".png\" or \".pnm\" file name extension for input file, trying anyway...\n");
            }
        } else {
            if (!(ext && ( !strcasecmp(ext,".flif")  || ( !strcasecmp(ext,".flf") )))) {
                e_printf("Warning: expected file name extension \".flif\" for input file, trying anyway...\n");
            }
        }
    } else if (argc>0) {
        e_printf("Input file does not exist: %s\n",argv[0]);
        return 1;
    }
    if (mode > 0 && argc > 2) {
        e_printf("Too many arguments.\n");
        return 1;
    }

#ifdef HAS_ENCODER
    if (mode == 0) {
        if (!handle_encode(argc, argv, images, palette_size, acb, method, lookback, learn_repeats, frame_delay)) return 2;
    } else if (mode == 1) {
#endif
        return handle_decode(argv, images, quality, scale);
#ifdef HAS_ENCODER
    } else if (mode == 2) {
        if (scale > 1) {e_printf("Not yet supported: transcoding downscaled image; use decode + encode!\n");}
        if (!decode_flif(argv, images, quality, scale)) return 2;
        argc--; argv++;
        if (!encode_flif(argc, argv, images, palette_size, acb, method, lookback, learn_repeats, frame_delay)) return 2;
    }
#endif
    return 0;
}

