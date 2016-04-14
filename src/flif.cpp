/*
 FLIF - Free Lossless Image Format
 Copyright (C) 2010-2016  Jon Sneyers & Pieter Wuille, LGPL v3+

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License
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

void show_help(int mode) {
    // mode = 0: only encode options
    // mode = 1: only decode options
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
    v_printf(1,"   -h, --help                  show help (use -hvv for advanced options)\n");
    v_printf(1,"   -v, --verbose               increase verbosity (multiple -v for more output)\n");
    v_printf(2,"   -c, --no-crc                don't verify the CRC (or don't add a CRC)\n");
#ifdef HAS_ENCODER
    if (mode != 1) {
    v_printf(1,"Encode options: (-e, --encode)\n");
    v_printf(1,"   -E, --effort=N              0=fast/poor compression, 100=slowest/best? (default: -E60)\n");
    v_printf(1,"   -I, --interlace             interlacing (default, except for tiny images)\n");
    v_printf(1,"   -N, --no-interlace          force no interlacing\n");
    v_printf(1,"   -Q, --lossy=N               lossy compression; default: -Q100 (lossless)\n");
    v_printf(1,"   -K, --keep-invisible-rgb    store original RGB values behind A=0\n");
    v_printf(1,"   -F, --frame-delay=N[,N,..]  delay between animation frames in ms; default: -F100\n");
//    v_printf(1,"Multiple input images (for animated FLIF) must have the same dimensions.\n");
    v_printf(2,"Advanced encode options: (mostly useful for flifcrushing)\n");
    v_printf(2,"   -P, --max-palette-size=N    max size for Palette(_Alpha); default: -P%i\n",DEFAULT_MAX_PALETTE_SIZE);
    v_printf(2,"   -A, --force-color-buckets   force Color_Buckets transform\n");
    v_printf(2,"   -B, --no-color-buckets      disable Color_Buckets transform\n");
    v_printf(2,"   -C, --no-channel-compact    disable Channel_Compact transform\n");
    v_printf(2,"   -Y, --no-ycocg              disable YCoCg transform (use plain RGB instead)\n");
    v_printf(2,"   -S, --no-frame-shape        disable Frame_Shape transform\n");
    v_printf(2,"   -L, --max-frame-lookback=N  max nb of frames for Frame_Lookback; default: -L1\n");
    v_printf(2,"   -R, --maniac-repeats=N      MANIAC learning iterations; default: -R%i\n",TREE_LEARN_REPEATS);
    v_printf(3,"   -T, --maniac-threshold=N    MANIAC tree growth split threshold, in bits saved; default: -T%i\n",CONTEXT_TREE_SPLIT_THRESHOLD/5461);
    v_printf(3,"   -D, --maniac-divisor=N      MANIAC inner node count divisor; default: -D%i\n",CONTEXT_TREE_COUNT_DIV);
    v_printf(3,"   -M, --maniac-min-size=N     MANIAC post-pruning threshold; default: -M%i\n",CONTEXT_TREE_MIN_SUBTREE_SIZE);
    v_printf(3,"   -X, --chance-cutoff=N       minimum chance (N/4096); default: -X2\n");
    v_printf(3,"   -Z, --chance-alpha=N        chance decay factor; default: -Z19\n");
    v_printf(3,"   -U, --adaptive              adaptive lossy, second input image is saliency map\n");
    v_printf(3,"   -G, --guess=N[N..]          pixel predictor for each plane (Y,Co,Cg,Alpha,Lookback)\n");
    v_printf(3,"                               ?=pick heuristically, 0=avg, 1=median_grad, 2=median_nb, 3=mixed\n");
    v_printf(3,"   -H, --invisible-guess=N     predictor for invisible pixels (only if -K is not used)\n");
    }
#endif
    if (mode != 0) {
    v_printf(1,"Decode options: (-d, --decode)\n");
    v_printf(1,"   -i, --identify             do not decode, just identify the input FLIF file\n");
    v_printf(1,"   -q, --quality=N            lossy decode quality percentage; default -q100\n");
    v_printf(1,"   -s, --scale=N              lossy downscaled image at scale 1:N (2,4,8,16,32); default -s1\n");
    v_printf(1,"   -r, --resize=WxH           lossy downscaled image to fit inside WxH (but typically smaller)\n");
    }
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
    v_printf(3," (___ | | | ___)   ");v_printf(2,"FLIF (Free Lossless Image Format) 0.2.0rc16 [14 Apr 2016]\n");
    v_printf(3,"  (__ | |_| __)    ");v_printf(3,"Copyright (C) 2016 Jon Sneyers and Pieter Wuille\n");
    v_printf(3,"    (_|___|_)      ");
#ifdef HAS_ENCODER
    v_printf(3,"License LGPLv3+: GNU LGPL version 3 or later\n");
#else
    v_printf(3,"License Apache 2.0 (compiled with decoder only)\n");
#endif
    v_printf(3,"\n");
    v_printf(4,"This is free software: you are free to change and redistribute it.\n");
    v_printf(4,"There is NO WARRANTY, to the extent permitted by law.\n");
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
        v_printf_tty(2,"\r");
        if (!image.load(argv[0])) {
            e_printf("Could not read input file: %s\n", argv[0]);
            return false;
        };
        images.push_back(std::move(image));
        Image& last_image = images.back();
        if (last_image.rows() != images[0].rows() || last_image.cols() != images[0].cols()) {
            e_printf("Dimensions of all input images should be the same!\n");
            e_printf("  First image is %ux%u\n",images[0].cols(),images[0].rows());
            e_printf("  This image is %ux%u: %s\n",last_image.cols(),last_image.rows(),argv[0]);
            return false;
        }
        if (last_image.numPlanes() < images[0].numPlanes()) {
            if (images[0].numPlanes() == 3) last_image.ensure_chroma();
            else if (images[0].numPlanes() == 4) last_image.ensure_alpha();
            else { e_printf("Problem while loading input images, please report this.\n"); return false; }
        } else if (last_image.numPlanes() > images[0].numPlanes()) {
            if (last_image.numPlanes() == 3) { for (Image& i : images) i.ensure_chroma(); }
            else if (last_image.numPlanes() == 4) { for (Image& i : images) i.ensure_alpha(); }
            else { e_printf("Problem while loading input images, please report this.\n"); return false; }
        }
        argc--; argv++;
        if (nb_input_images>1) {v_printf(2,"    (%i/%i)         ",(int)images.size(),nb_input_images); v_printf(4,"\n");}
    }
    return true;
}
bool encode_flif(int argc, char **argv, Images &images, int palette_size, int acb, flifEncodingOptional method, int lookback,
                 int learn_repeats, std::vector<int> &frame_delay, int divisor=CONTEXT_TREE_COUNT_DIV, int min_size=CONTEXT_TREE_MIN_SUBTREE_SIZE,
                 int split_threshold=CONTEXT_TREE_SPLIT_THRESHOLD, int yiq=1, int plc=1, int frs=1, int cutoff=2, int alpha=19, int crc_check=-1, int loss=0, int predictor[]=NULL, int invisible_predictor=2) {
    bool flat=true;
    unsigned int framenb=0;
    for (Image& i : images) { i.frame_delay = frame_delay[framenb]; if (framenb+1 < frame_delay.size()) framenb++; }
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
      if (plc && !loss) {
        desc.push_back("Channel_Compact");  // compactify channels (not if lossy, because then loss gets magnified!)
      }
      if (yiq) {
        desc.push_back("YCoCg");  // convert RGB(A) to YCoCg(A)
      }
      desc.push_back("PermutePlanes");  // permute RGB to GRB
      desc.push_back("Bounds");  // get the bounds of the color spaces
    }
    if (!loss) {
    // only use palette/CB if we're lossless, because lossy and palette don't go well together...
    if (palette_size == -1) {
        palette_size = DEFAULT_MAX_PALETTE_SIZE;
        if (nb_pixels * images.size() / 3 < DEFAULT_MAX_PALETTE_SIZE) {
          palette_size = nb_pixels * images.size() / 3;
        }
    }
    if (palette_size != 0) {
        desc.push_back("Palette_Alpha");  // try palette (including alpha)
        desc.push_back("Palette");  // try palette (without alpha)
    }
    if (acb == -1) {
      // not specified if ACB should be used
      if (nb_pixels * images.size() > 10000) {
        desc.push_back("Color_Buckets");  // try auto color buckets on large images
      }
    } else if (acb) {
      desc.push_back("Color_Buckets");  // try auto color buckets if forced
    }
    }
    if (method.o == Optional::undefined) {
        // no method specified, pick one heuristically
        if (nb_pixels * images.size() < 10000) method.encoding=flifEncoding::nonInterlaced; // if the image is small, not much point in doing interlacing
        else method.encoding=flifEncoding::interlaced; // default method: interlacing
    }
    if (images.size() > 1) {
        desc.push_back("Duplicate_Frame");  // find duplicate frames
        if (!loss) { // only if lossless
          if (frs) desc.push_back("Frame_Shape");  // get the shapes of the frames
          if (lookback) desc.push_back("Frame_Lookback");  // make a "deep" alpha channel (negative values are transparent to some previous frame)
        }
    }
    if (learn_repeats < 0) {
        // no number of repeats specified, pick a number heuristically
        learn_repeats = TREE_LEARN_REPEATS;
        //if (nb_pixels * images.size() < 5000) learn_repeats--;        // avoid large trees for small images
        if (learn_repeats < 0) learn_repeats=0;
    }
    FILE *file = fopen(argv[0],"wb");
    if (!file)
        return false;
    FileIO fio(file, argv[0]);
    return flif_encode(fio, images, desc, method.encoding, learn_repeats, acb, palette_size, lookback, divisor, min_size, split_threshold, cutoff, alpha, crc_check, loss, predictor, invisible_predictor);
}

bool handle_encode(int argc, char **argv, Images &images, int palette_size, int acb, flifEncodingOptional method,
                   int lookback, int learn_repeats, std::vector<int> &frame_delay, int divisor=CONTEXT_TREE_COUNT_DIV,
                   int min_size=CONTEXT_TREE_MIN_SUBTREE_SIZE, int split_threshold=CONTEXT_TREE_SPLIT_THRESHOLD,
                   int yiq=1, int plc=1, bool alpha_zero_special=true, int frs=1, int cutoff=2, int alpha=19, int crc_check=-1, int loss=0, int predictor[]=NULL, int invisible_predictor=2) {
    if (!encode_load_input_images(argc,argv,images)) return false;
    if (!alpha_zero_special) for (Image& i : images) i.alpha_zero_special = false;
    argv += (argc-1);
    argc = 1;
    return encode_flif(argc, argv, images, palette_size, acb, method, lookback, learn_repeats, frame_delay, divisor, min_size, split_threshold, yiq, plc, frs, cutoff, alpha, crc_check, loss, predictor, invisible_predictor);
}
#endif

bool decode_flif(char **argv, Images &images, int quality, int scale, int resize_width, int resize_height, int crc_check) {
    FILE *file = fopen(argv[0],"rb");
    if(!file) return false;
    FileIO fio(file, argv[0]);
    return flif_decode(fio, images, quality, scale, resize_width, resize_height, crc_check);
}

int handle_decode(int argc, char **argv, Images &images, int quality, int scale, int resize_width, int resize_height, int crc_check) {
    if (scale < 0) {
        // just identify the file(s), don't actually decode
        while (argc>0) {
            decode_flif(argv, images, quality, scale, resize_width, resize_height, crc_check);
            argv++; argc--;
        }
        return 0;
    }
    char *ext = strrchr(argv[1],'.');
    if (!check_compatible_extension(ext) && strcmp(argv[1],"null:")) {
        e_printf("Error: expected \".png\", \".pnm\" or \".pam\" file name extension for output file\n");
        return 1;
    }
    if (!decode_flif(argv, images, quality, scale, resize_width, resize_height, crc_check)) {
        e_printf("Error: could not decode FLIF file\n"); return 3;
    }
    if (!strcmp(argv[1],"null:")) return 0;
//    if (scale>1)
//        v_printf(3,"Downscaling output: %ux%u -> %ux%u\n",images[0].cols(),images[0].rows(),images[0].cols()/scale,images[0].rows()/scale);
    if (images.size() == 1) {
        if (!images[0].save(argv[1])) return 2;
    } else {
        int counter=0;
        std::vector<char> vfilename(strlen(argv[1])+6);
        char *filename = &vfilename[0];
        strcpy(filename,argv[1]);
        char *a_ext = strrchr(filename,'.');
        for (Image& image : images) {
            sprintf(a_ext,"-%03d%s",counter++,ext);
            if (!image.save(filename)) return 2;
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
    int mode = -1; // 0 = encode, 1 = decode, 2 = transcode
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
    int loss=0;
    bool adaptive=false;
    int predictor[]={-2,-2,-2,-2,-2}; // heuristically pick a fixed predictor on all planes
    int invisible_predictor=2;
#else
    int mode = 1;
#endif
    int crc_check = -1;
    int quality = 100; // 100 = everything, positive value: partial decode, negative value: only rough data
    int scale = 1;
    int resize_width = 0, resize_height = 0;
    bool showhelp = false;
    if (strcmp(argv[0],"cflif") == 0) mode = 0;
    if (strcmp(argv[0],"dflif") == 0) mode = 1;
    if (strcmp(argv[0],"deflif") == 0) mode = 1;
    if (strcmp(argv[0],"decflif") == 0) mode = 1;
    static struct option optlist[] = {
        {"help", 0, NULL, 'h'},
        {"decode", 0, NULL, 'd'},
        {"verbose", 0, NULL, 'v'},
        {"no-crc", 0, NULL, 'c'},
        {"quality", 1, NULL, 'q'},
        {"scale", 1, NULL, 's'},
        {"resize", 1, NULL, 'r'},
        {"identify", 0, NULL, 'i'},
        {"version", 0, NULL, 'V'},
#ifdef HAS_ENCODER
        {"encode", 0, NULL, 'e'},
        {"transcode", 0, NULL, 't'},
        {"interlace", 0, NULL, 'I'},
        {"no-interlace", 0, NULL, 'N'},
        {"frame-delay", 1, NULL, 'F'},
        {"keep-invisible-rgb", 0, NULL, 'K'},
        {"max-palette-size", 1, NULL, 'P'},
        {"force-color-buckets", 0, NULL, 'A'},
        {"no-color-buckets", 0, NULL, 'B'},
        {"no-ycocg", 0, NULL, 'Y'},
        {"no-channel-compact", 0, NULL, 'C'},
        {"max-frame-lookback", 1, NULL, 'L'},
        {"no-frame-shape", 0, NULL, 'S'},
        {"maniac-repeats", 1, NULL, 'R'},
        {"maniac-divisor", 1, NULL, 'D'},
        {"maniac-min-size", 1, NULL, 'M'},
        {"maniac-threshold", 1, NULL, 'T'},
        {"chance-cutoff", 1, NULL, 'X'},
        {"chance-alpha", 1, NULL, 'Z'},
        {"lossy", 1, NULL, 'Q'},
        {"adaptive", 0, NULL, 'U'},
        {"guess", 1, NULL, 'G'},
        {"invisible-guess", 1, NULL, 'H'},
        {"effort", 1, NULL, 'E'},
#endif
        {0, 0, 0, 0}
    };
    int i,c;
#ifdef HAS_ENCODER
    while ((c = getopt_long (argc, argv, "hdvciVq:s:r:etINnF:KP:ABYCL:SR:D:M:T:X:Z:Q:UG:H:E:", optlist, &i)) != -1) {
#else
    while ((c = getopt_long (argc, argv, "hdvciVq:s:r:", optlist, &i)) != -1) {
#endif
        switch (c) {
        case 'd': mode=1; break;
        case 'v': increase_verbosity(); break;
        case 'V': increase_verbosity(3); break;
        case 'c': crc_check = 0; break;
        case 'q': quality=atoi(optarg);
                  if (quality < 0 || quality > 100) {e_printf("Not a sensible number for option -q\n"); return 1; }
                  break;
        case 's': scale=atoi(optarg);
                  if (scale < 1 || scale > 128) {e_printf("Not a sensible number for option -s\n"); return 1; }
                  break;
        case 'r': if (sscanf(optarg,"%ix%i", &resize_width, &resize_height) < 1) {e_printf("Not a sensible value for option -r (expected WxH)\n"); return 1; }
                  if (!resize_height) resize_height = resize_width;
                  break;
        case 'i': scale = -1; break;
#ifdef HAS_ENCODER
        case 'e': mode=0; break;
        case 't': mode=2; break;
        case 'I': method.encoding=flifEncoding::interlaced; break;
        case 'n': // undocumented: lower case -n still works
        case 'N': method.encoding=flifEncoding::nonInterlaced; break;
        case 'A': acb=1; break;
        case 'B': acb=0; break;
        case 'P': palette_size=atoi(optarg);
                  if (palette_size < -32000 || palette_size > 32000) {e_printf("Not a sensible number for option -P\n"); return 1; }
                  if (palette_size > 512) {v_printf(1,"Warning: palette size above 512 implies that simple FLIF decoders (8-bit only) cannot decode this file.\n"); }
                  if (palette_size == 0) {v_printf(5,"Palette disabled\n"); }
                  break;
        case 'R': learn_repeats=atoi(optarg);
                  if (learn_repeats < 0 || learn_repeats > 20) {e_printf("Not a sensible number for option -R\n"); return 1; }
                  break;
        case 'F': frame_delay.clear();
                  while(optarg != 0) {
                    int d=strtol(optarg,&optarg,10);
                    if (d==0) break;
                    if (*optarg == ',' || *optarg == '+' || *optarg == ' ') optarg++;
                    frame_delay.push_back(d);
                    if (d < 0 || d > 60000) {e_printf("Not a sensible number for option -F: %i\n",d); return 1; }
                  }
                  if (frame_delay.size() < 1) frame_delay.push_back(100);
                  break;
        case 'L': lookback=atoi(optarg);
                  if (lookback < -1 || lookback > 256) {e_printf("Not a sensible number for option -L\n"); return 1; }
                  break;
        case 'D': divisor=atoi(optarg);
                  if (divisor <= 0 || divisor > 0xFFFFFFF) {e_printf("Not a sensible number for option -D\n"); return 1; }
                  break;
        case 'M': min_size=atoi(optarg);
                  if (min_size < 0) {e_printf("Not a sensible number for option -M\n"); return 1; }
                  break;
        case 'T': split_threshold=atoi(optarg);
                  if (split_threshold <= 3 || split_threshold > 100000) {e_printf("Not a sensible number for option -T\n"); return 1; }
                  split_threshold *= 5461;
                  break;
        case 'Y': yiq=0; break;
        case 'C': plc=0; break;
        case 'S': frs=0; break;
        case 'K': alpha_zero_special=false; break;
        case 'X': cutoff=atoi(optarg);
                  if (cutoff < 1 || cutoff > 128) {e_printf("Not a sensible number for option -X (try something between 1 and 128)\n"); return 1; }
                  break;
        case 'Z': alpha=atoi(optarg);
                  if (alpha < 2 || alpha > 128) {e_printf("Not a sensible number for option -Z (try something between 2 and 128)\n"); return 1; }
                  break;
        case 'Q': loss=100-atoi(optarg);
                  // can't go above quality 100 = lossless
                  // can go way below 0 if you want
                  if (loss < 0) {e_printf("Not a sensible number for option -Q (try something between 0 and 100)\n"); return 1; }
                  break;
        case 'U': adaptive=true; break;
        case 'G': {
                  int p=0;
                  while(*optarg != 0) {
                    int d=0;
                    switch (*optarg) {
                        case '?': case 'G': case 'g': case 'H': case 'h': // guess/heuristically choose
                            d = -2; break;
                        case '0': case 'A': case 'a': // average
                            d = 0; break;
                        case '1': case 'M': case 'm': // median of 3 values (avg, grad1, grad2)
                            d = 1; break;
                        case '2': case 'N': case 'n': // median of 3 neighbors
                            d = 2; break;
                        case '3': // auto/mixed, usually a bad idea
                            d = -1; break;
                        case ' ': case ',': case '+':
                            optarg++; continue;
                        default:
                        e_printf("Not a sensible value for option -G\nValid values are: 0 (avg), 1 (median avg/gradients), 2 (median neighbors), 3 (auto/mixed), ? (heuristically pick 0-2)\n"); return 1;
                    }
                    if (p>4) {e_printf("Error while parsing option -G: too many planes specified\n"); return 1; }
                    predictor[p] = d;
                    p++; optarg++;
                  }
                  for (; p<5; p++) predictor[p]=predictor[0];
                  }
                  break;
        case 'H': invisible_predictor=atoi(optarg);
                  if (invisible_predictor < 0 || invisible_predictor > 2) {e_printf("Not a sensible value for option -H\nValid values are: 0 (avg), 1 (median avg/gradients), 2 (median neighbors)\n"); return 1; }
                  break;
        case 'E': {
                  int effort=atoi(optarg);
                  if (effort < 0 || effort > 100) {e_printf("Not a sensible number for option -E (try something between 0 and 100)\n"); return 1; }
                  // set some parameters automatically
                  if (effort < 10) learn_repeats=0;
                  else if (effort <= 50) {learn_repeats=1; split_threshold=5461*8*5;}
                  else if (effort <= 70) {learn_repeats=2;  split_threshold=5461*8*8;}
                  else if (effort <= 90) {learn_repeats=3; split_threshold=5461*8*10;}
                  else if (effort <= 100) {learn_repeats=4; split_threshold=5461*8*12;}
                  if (effort < 15) { for (int i=0; i<5; i++) predictor[i]=0; }
                  else if (effort < 30) { predictor[1]=0; predictor[2]=0; }
                  if (effort < 5) acb=0;
                  if (effort < 8) palette_size=0;
                  if (effort < 25) plc=0;
                  if (effort < 30) lookback=0;
                  if (effort < 5) frs=0;
                  v_printf(3,"Encode effort: %i, corresponds to parameters -R%i -T%i%s%s%s%s\n", effort, learn_repeats, split_threshold/5461,
                                    (effort<15?" -G0":(effort<30?" -G?00":"")), (effort<5?" -B":""), (effort<8?" -P0":""), (effort<25?" -C":""));
                                    // not mentioning animation options since they're usually irrelevant
                  }
                  break;
#endif
        case 'h': showhelp=true; break;
        default: show_help(mode); return 0;
        }
    }
    argc -= optind;
    argv += optind;

    show_banner();
    if (argc == 0 || showhelp) {
        if (get_verbosity() == 1 || showhelp) show_help(mode);
        return 0;
    }

    if (argc == 1 && scale != -1) {
        show_help(mode);
        e_printf("\nOutput file missing.\n");
        return 1;
    }
    if (scale == -1) mode = 1;
    if (mode < 0) mode = 0;
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
    if (adaptive) loss = -loss; // use negative loss to indicate we want to do adaptive lossy encoding
    if (mode == 0) {
        if (!handle_encode(argc, argv, images, palette_size, acb, method, lookback, learn_repeats, frame_delay, divisor, min_size, split_threshold, yiq, plc, alpha_zero_special, frs, cutoff, alpha, crc_check, loss, predictor, invisible_predictor)) return 2;
    } else if (mode == 1) {
#endif
        return handle_decode(argc, argv, images, quality, scale, resize_width, resize_height, crc_check);
#ifdef HAS_ENCODER
    } else if (mode == 2) {
//        if (scale > 1) {e_printf("Not yet supported: transcoding downscaled image; use decode + encode!\n");}
        if (!decode_flif(argv, images, quality, scale, resize_width, resize_height, crc_check)) return 2;
        argc--; argv++;
        if (!encode_flif(argc, argv, images, palette_size, acb, method, lookback, learn_repeats, frame_delay, divisor, min_size, split_threshold, yiq, plc, frs, cutoff, alpha, crc_check, loss, predictor, invisible_predictor)) return 2;
    }
#endif
    return 0;
}

