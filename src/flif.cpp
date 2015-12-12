/*
 FLIF - Free Lossless Image Format
 Copyright (C) 2010-2015  Jon Sneyers & Pieter Wuille, GPL v3+

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

#include "maniac/rac.hpp"
#include "maniac/compound.hpp"
#include "maniac/util.hpp"

#include "image/color_range.hpp"
#include "transform/factory.hpp"

#include "flif_config.h"

#include <getopt.h>

#include <stdarg.h>

#include "common.hpp"
#ifdef HAS_ENCODER
#include "flif-enc.hpp"
#endif
#include "flif-dec.hpp"

#ifdef _MSC_VER
#define strcasecmp stricmp
#endif





// planes:
// 0    Y channel (luminance)
// 1    I (chroma)
// 2    Q (chroma)
// 3    Alpha (transparency)
// 4    frame lookbacks ("deep alpha", only for animations)

/******************************************/
/*   scanlines encoding/decoding          */
/******************************************/



/******************************************/
/*   FLIF2 encoding/decoding              */
/******************************************/
void show_help() {
    v_printf(1,"Usage:\n");
#ifdef HAS_ENCODER
    v_printf(1,"   flif [-e] [encode options] <input image(s)> <output.flif>\n");
#endif
    v_printf(1,"   flif [-d] [decode options] <input.flif> <output.pnm | output.pam | output.png>\n");
#ifdef HAS_ENCODER
    v_printf(2,"   flif [-t] [decode options] [encode options] <input.flif> <output.flif>\n");
#endif
    v_printf(1,"Supported input/output image formats: PNG, PNM (PPM,PGM,PBM), PAM\n");
    v_printf(1,"General Options:\n");
    v_printf(1,"   -h, --help               show help (use -hvv for advanced options)\n");
    v_printf(1,"   -v, --verbose            increase verbosity (multiple -v for more output)\n");
#ifdef HAS_ENCODER
    v_printf(1,"Encode options:\n");
    v_printf(1,"   -i, --interlace          interlacing (default, except for tiny images)\n");
    v_printf(1,"   -n, --no-interlace       force no interlacing\n");
    v_printf(1,"   -f, --frame-delay=N      delay between animation frames, in ms; default: -f100\n");
    v_printf(1,"   -A, --keep-alpha-zero    store original RGB values behind A=0\n");
//    v_printf(1,"Multiple input images (for animated FLIF) must have the same dimensions.\n");
    v_printf(2,"Advanced encode options: (mostly useful for flifcrushing)\n");
    v_printf(2,"   -a, --acb                force auto color buckets (ACB)\n");
    v_printf(2,"   -b, --no-acb             force no auto color buckets\n");
    v_printf(2,"   -p, --palette=N          max palette size for PLT and PLA; default: -p1024\n");
    v_printf(2,"   -C, --no-plc             disable PLC transform (no channel compactification)\n");
    v_printf(2,"   -R, --rgb                disable YIQ transform (use plain RGB instead)\n");
    v_printf(2,"   -F, --no-frs             disable FRS transform (no frame shaping)\n");
    v_printf(2,"   -l, --lookback=N         max lookback between animation frames (for FRA); default: -l1\n");
    v_printf(2,"   -r, --repeats=N          MANIAC learning iterations; default: -r%i\n",TREE_LEARN_REPEATS);
    v_printf(3,"   -S, --split-threshold=N  MANIAC tree growth criterion, in bits saved; default: -S%i\n",CONTEXT_TREE_SPLIT_THRESHOLD/5461);
    v_printf(3,"   -D, --divisor=N          MANIAC inner node count divisor; default: -D%i\n",CONTEXT_TREE_COUNT_DIV);
    v_printf(3,"   -M, --min-size=N         MANIAC post-pruning threshold; default: -M%i\n",CONTEXT_TREE_MIN_SUBTREE_SIZE);
    v_printf(3,"   -X, --rac-cutoff=N       RAC minimum chance (N/4096); default: -X2\n");
    v_printf(3,"   -Y, --rac-alpha=N        RAC decay factor; default: -Y19\n");
#endif
    v_printf(1,"Decode options:\n");
    v_printf(1,"   -I, --identify           do not decode, just identify the input FLIF file\n");
    v_printf(1,"   -q, --quality=N          lossy decode quality percentage; default -q100\n");
    v_printf(1,"   -s, --scale=N            lossy downscaled image at scale 1:N (2,4,8,16,32); default -s1\n");
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
    v_printf(3,"  ____ _(_)____\n");
    v_printf(3," (___ | | | ___)   ");v_printf(2,"FLIF (Free Lossless Image Format) 0.2.0rc3 [8 December 2015]\n");
    v_printf(3,"  (__ | |_| __)    ");v_printf(2,"Copyright (C) 2015 Jon Sneyers and Pieter Wuille\n");
    v_printf(3,"    (_|___|_)      ");v_printf(2,"License GPLv3+: GNU GPL version 3 or later\n");
    v_printf(3,"\n");
    v_printf(2,"This is free software: you are free to change and redistribute it.\n");
    v_printf(2,"There is NO WARRANTY, to the extent permitted by law.\n");
#ifndef HAS_ENCODER
    v_printf(2,"Non-default compile-option: DECODER ONLY\n");
#endif
#ifndef SUPPORT_HDR
    v_printf(2,"Non-default compile-option: 8-BIT ONLY\n");
#endif
#ifndef FAST_BUT_WORSE_COMPRESSION
    v_printf(2,"Non-default compile-option: SLOWER, BETTER COMPRESSION (CANNOT ENCODE/DECODE NORMAL FLIF FILES!)\n");
#endif
}

bool check_compatible_extension (char *ext) {
    if (!(ext && ( !strcasecmp(ext,".png") // easy to add or remove formats
                  || !strcasecmp(ext,".pnm")
                  || !strcasecmp(ext,".ppm")
                  || !strcasecmp(ext,".pgm")
                  || !strcasecmp(ext,".pbm")
                  || !strcasecmp(ext,".pam")
                  || !strcasecmp(ext,".rggb")
                  || !strcasecmp(ext,"null:")
                  ))) {
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
bool encode_flif(int argc, char **argv, Images &images, int palette_size, int acb, flifEncodingOptional method, int lookback, int learn_repeats, int divisor=CONTEXT_TREE_COUNT_DIV, int min_size=CONTEXT_TREE_MIN_SUBTREE_SIZE, int split_threshold=CONTEXT_TREE_SPLIT_THRESHOLD, int yiq=1, int plc=1, int frs=1, int cutoff=2, int alpha=19) {
    bool flat=true;
    for (Image &image : images) if (image.uses_alpha()) flat=false;
    if (flat && images[0].numPlanes() == 4) {
        v_printf(2,"Alpha channel not actually used, dropping it.\n");
        for (Image &image : images) image.drop_alpha();
    }
    bool grayscale=true;
    for (Image &image : images) if (image.uses_color()) grayscale=false;
    if (grayscale && images[0].numPlanes() == 3) {
        v_printf(2,"Chroma not actually used, dropping it.\n");
        for (Image &image : images) image.drop_color();
    }
    uint64_t nb_pixels = (uint64_t)images[0].rows() * images[0].cols();
    std::vector<std::string> desc;
    if (nb_pixels > 2) {         // no point in doing anything for 1- or 2-pixel images
      if (plc) {
        desc.push_back("PLC");  // compactify channels
      }
      if (yiq) {
        desc.push_back("YIQ");  // convert RGB(A) to YIQ(A)
      }
      desc.push_back("BND");  // get the bounds of the color spaces
    }
    if (palette_size == -1) {
        palette_size = 1024;
        if (nb_pixels * images.size() / 2 < 1024) {
          palette_size = nb_pixels * images.size() / 2;
        }
    }
    if (palette_size != 0) {
        desc.push_back("PLA");  // try palette (including alpha)
    }
    if (palette_size != 0) {
        desc.push_back("PLT");  // try palette (without alpha)
    }

    if (acb == -1) {
      // not specified if ACB should be used
      if (nb_pixels * images.size() > 10000) {
        desc.push_back("ACB");  // try auto color buckets on large images
      }
    } else if (acb) {
      desc.push_back("ACB");  // try auto color buckets if forced
    }
    if (method.o == Optional::undefined) {
        // no method specified, pick one heuristically
        if (nb_pixels * images.size() < 10000) method.encoding=flifEncoding::nonInterlaced; // if the image is small, not much point in doing interlacing
        else method.encoding=flifEncoding::interlaced; // default method: interlacing
    }
    if (images.size() > 1) {
        desc.push_back("DUP");  // find duplicate frames
        if (frs) desc.push_back("FRS");  // get the shapes of the frames
        if (lookback) desc.push_back("FRA");  // make a "deep" alpha channel (negative values are transparent to some previous frame)
    }
    if (learn_repeats < 0) {
        // no number of repeats specified, pick a number heuristically
        learn_repeats = TREE_LEARN_REPEATS;
        if (nb_pixels * images.size() < 5000) learn_repeats--;        // avoid large trees for small images
        if (learn_repeats < 0) learn_repeats=0;
    }
    FILE *file = fopen(argv[0],"wb");
    if (!file)
        return false;
    FileIO fio(file, argv[0]);
    return flif_encode(fio, images, desc, method.encoding, learn_repeats, acb, palette_size, lookback, divisor, min_size, split_threshold, cutoff, alpha);
}

bool handle_encode(int argc, char **argv, Images &images, int palette_size, int acb, flifEncodingOptional method,
                   int lookback, int learn_repeats, std::vector<int> &frame_delay, int divisor=CONTEXT_TREE_COUNT_DIV,
                   int min_size=CONTEXT_TREE_MIN_SUBTREE_SIZE, int split_threshold=CONTEXT_TREE_SPLIT_THRESHOLD,
                   int yiq=1, int plc=1, bool alpha_zero_special=true, int frs=1, int cutoff=2, int alpha=19) {
    if (!encode_load_input_images(argc,argv,images)) return false;
    unsigned int framenb=0;
    for (Image& i : images) { i.frame_delay = frame_delay[framenb]; if (framenb+1 < frame_delay.size()) framenb++; }
    if (!alpha_zero_special) for (Image& i : images) i.alpha_zero_special = false;
    argv += (argc-1);
    argc = 1;
    return encode_flif(argc, argv, images, palette_size, acb, method, lookback, learn_repeats, divisor, min_size, split_threshold, yiq, plc, frs, cutoff, alpha);
}
#endif

bool decode_flif(char **argv, Images &images, int quality, int scale) {
    FILE *file = fopen(argv[0],"rb");
    if(!file) return false;
    FileIO fio(file, argv[0]);
    return flif_decode(fio, images, quality, scale);
}

int handle_decode(int argc, char **argv, Images &images, int quality, int scale) {
    if (scale < 0) {
        // just identify the file(s), don't actually decode
        while (argc>0) {
            decode_flif(argv, images, quality, scale);
            argv++; argc--;
        }
        return 0;
    }
    char *ext = strrchr(argv[1],'.');
    if (!check_compatible_extension(ext) && strcmp(argv[1],"null:")) {
        e_printf("Error: expected \".png\", \".pnm\" or \".pam\" file name extension for output file\n");
        return 1;
    }
    if (!decode_flif(argv, images, quality, scale)) {
        e_printf("Error: could not decode FLIF file\n"); return 3;
    }
    if (!strcmp(argv[1],"null:")) return 0;
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
    std::vector<int> frame_delay;
    frame_delay.push_back(100);
    int palette_size = -1;
    int lookback = 1;
    int divisor=CONTEXT_TREE_COUNT_DIV;
    int min_size=CONTEXT_TREE_MIN_SUBTREE_SIZE;
    int split_threshold=CONTEXT_TREE_SPLIT_THRESHOLD;
    int yiq = 1;
    int plc = 1;
    int frs = 1;
    bool alpha_zero_special = true;
    int alpha=19;
    int cutoff=2;
#else
    int mode = 1;
#endif
    int quality = 100; // 100 = everything, positive value: partial decode, negative value: only rough data
    int scale = 1;
    bool showhelp = false;
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
        {"identify", 0, NULL, 'I'},
        {"version", 0, NULL, 'V'},
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
        {"divisor", 1, NULL, 'D'},
        {"min-size", 1, NULL, 'M'},
        {"split-threshold", 1, NULL, 'S'},
        {"rgb", 0, NULL, 'R'},
        {"no-plc", 0, NULL, 'C'},
        {"no-frs", 0, NULL, 'F'},
        {"keep-alpha-zero", 0, NULL, 'A'},
        {"rac-cutoff", 1, NULL, 'X'},
        {"rac-alpha", 1, NULL, 'Y'},
#endif
        {0, 0, 0, 0}
    };
    int i,c;
#ifdef HAS_ENCODER
    while ((c = getopt_long (argc, argv, "hedtvIVinabq:s:p:r:f:l:D:M:S:RCFAX:Y:", optlist, &i)) != -1) {
#else
    while ((c = getopt_long (argc, argv, "hdvIVq:s:", optlist, &i)) != -1) {
#endif
        switch (c) {
        case 'd': mode=1; break;
        case 'v': increase_verbosity(); break;
        case 'V': increase_verbosity(); break;
        case 'q': quality=atoi(optarg);
                  if (quality < 0 || quality > 100) {e_printf("Not a sensible number for option -q\n"); return 1; }
                  break;
        case 's': scale=atoi(optarg);
                  if (scale < 1 || scale > 128) {e_printf("Not a sensible number for option -s\n"); return 1; }
                  break;
        case 'I': scale = -1; break;
#ifdef HAS_ENCODER
        case 'e': mode=0; break;
        case 't': mode=2; break;
        case 'i': method.encoding=flifEncoding::interlaced; break;
        case 'n': method.encoding=flifEncoding::nonInterlaced; break;
        case 'a': acb=1; break;
        case 'b': acb=0; break;
        case 'p': palette_size=atoi(optarg);
                  if (palette_size < -30000 || palette_size > 30000) {e_printf("Not a sensible number for option -p\n"); return 1; }
                  if (palette_size == 0) {v_printf(5,"Palette disabled\n"); }
                  break;
        case 'r': learn_repeats=atoi(optarg);
                  if (learn_repeats < 0 || learn_repeats > 20) {e_printf("Not a sensible number for option -r\n"); return 1; }
                  break;
        case 'f': frame_delay.clear();
                  while(optarg != 0) {
                    int d=strtol(optarg,&optarg,10);
                    if (d==0) break;
                    if (*optarg == ',' || *optarg == '+') optarg++;
                    frame_delay.push_back(d);
                    if (d < 0 || d > 60000) {e_printf("Not a sensible number for option -f: %i\n",d); return 1; }
                  }
                  if (frame_delay.size() < 1) frame_delay.push_back(100);
                  break;
        case 'l': lookback=atoi(optarg);
                  if (lookback < -1 || lookback > 256) {e_printf("Not a sensible number for option -l\n"); return 1; }
                  break;
        case 'D': divisor=atoi(optarg);
                  if (divisor <= 0 || divisor > 0xFFFFFFF) {e_printf("Not a sensible number for option -D\n"); return 1; }
                  break;
        case 'M': min_size=atoi(optarg);
                  if (min_size < 0) {e_printf("Not a sensible number for option -M\n"); return 1; }
                  break;
        case 'S': split_threshold=atoi(optarg);
                  if (split_threshold <= 0 || split_threshold > 100000) {e_printf("Not a sensible number for option -S\n"); return 1; }
                  split_threshold *= 5461;
                  break;
        case 'R': yiq=0; break;
        case 'C': plc=0; break;
        case 'F': frs=0; break;
        case 'A': alpha_zero_special=false; break;
        case 'X': cutoff=atoi(optarg);
                  if (cutoff < 1 || cutoff > 128) {e_printf("Not a sensible number for option -X (try something between 1 and 128)\n"); return 1; }
                  break;
        case 'Y': alpha=atoi(optarg);
                  if (alpha < 4 || alpha > 128) {e_printf("Not a sensible number for option -Y (try something between 4 and 128)\n"); return 1; }
                  break;
#endif
        case 'h': showhelp=true; break;
        default: show_help(); return 0;
        }
    }
    argc -= optind;
    argv += optind;

    show_banner();
    if (argc == 0 || showhelp) {
        if (get_verbosity() == 1 || showhelp) show_help();
        return 0;
    }

    if (argc == 1 && scale != -1) {
        show_help();
        e_printf("\nOutput file missing.\n");
        return 1;
    }
    if (scale == -1) mode = 1;
    if (file_exists(argv[0])) {
        char *f = strrchr(argv[0],'/');
        char *ext = f ? strrchr(f,'.') : strrchr(argv[0],'.');
#ifdef HAS_ENCODER
        if (mode == 0 && file_is_flif(argv[0])) {
            char *f = strrchr(argv[1],'/');
            char *ext = f ? strrchr(f,'.') : strrchr(argv[1],'.');
            if ((ext && ( !strcasecmp(ext,".flif")  || ( !strcasecmp(ext,".flf") )))) {
                v_printf(3,"Input and output file are both FLIF file, adding implicit -t\n");
                mode = 2;
            } else {
                v_printf(3,"Input file is a FLIF file, adding implicit -d\n");
                mode = 1;
            }
        }
        if (mode != 0) {
#endif
            if (!(ext && ( !strcasecmp(ext,".flif")  || ( !strcasecmp(ext,".flf") )))) {
                e_printf("Warning: expected file name extension \".flif\" for input file, trying anyway...\n");
            }
#ifdef HAS_ENCODER
        } else {
            if (!check_compatible_extension(ext)) {
                e_printf("Warning: expected \".png\", \".pnm\" or \".pam\" file name extension for input file, trying anyway...\n");
            }
        }
#endif
    } else if (argc>0) {
        e_printf("Input file does not exist: %s\n",argv[0]);
        return 1;
    }
    if (mode > 0 && argc > 2 && scale != -1) {
        e_printf("Too many arguments.\n");
        return 1;
    }

#ifdef HAS_ENCODER
    if (mode == 0) {
        if (!handle_encode(argc, argv, images, palette_size, acb, method, lookback, learn_repeats, frame_delay, divisor, min_size, split_threshold, yiq, plc, alpha_zero_special, frs, cutoff, alpha)) return 2;
    } else if (mode == 1) {
#endif
        return handle_decode(argc, argv, images, quality, scale);
#ifdef HAS_ENCODER
    } else if (mode == 2) {
        if (scale > 1) {e_printf("Not yet supported: transcoding downscaled image; use decode + encode!\n");}
        if (!decode_flif(argv, images, quality, scale)) return 2;
        argc--; argv++;
        if (!encode_flif(argc, argv, images, palette_size, acb, method, lookback, learn_repeats, divisor, min_size, split_threshold, yiq, plc, frs, cutoff, alpha)) return 2;
    }
#endif
    return 0;
}

