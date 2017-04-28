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
#define strcasecmp _stricmp
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
    v_printf(2,"   -m, --no-metadata           strip Exif/XMP metadata (default is to keep it)\n");
    v_printf(2,"   -p, --no-color-profile      strip ICC color profile (default is to keep it)\n");
    v_printf(2,"   -o, --overwrite             overwrite existing files\n");
    v_printf(2,"   -k, --keep-palette          use input PNG palette / write palette PNG if possible\n");
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
    v_printf(2,"   -Y, --no-ycocg              disable YCoCg transform; use G(R-G)(B-G)\n");
    v_printf(2,"   -W, --no-subtract-green     disable YCoCg and SubtractGreen transform; use GRB\n");
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
    v_printf(3,"                               ?=pick heuristically, 0=avg, 1=median_grad, 2=median_nb, X=mixed\n");
    v_printf(3,"   -H, --invisible-guess=N     predictor for invisible pixels (only if -K is not used)\n");
    v_printf(3,"   -J, --chroma-subsample      write an incomplete 4:2:0 chroma subsampled FLIF file (lossy!)\n");
    }
#endif
    if (mode != 0) {
    v_printf(1,"Decode options: (-d, --decode)\n");
    v_printf(1,"   -i, --identify             do not decode, just identify the input FLIF file\n");
    v_printf(1,"   -q, --quality=N            lossy decode quality percentage; default -q100\n");
    v_printf(1,"   -s, --scale=N              lossy downscaled image at scale 1:N (2,4,8,16,32); default -s1\n");
    v_printf(1,"   -r, --resize=WxH           lossy downscaled image to fit inside WxH (but typically smaller)\n");
    v_printf(1,"   -f, --fit=WxH              lossy downscaled image to exactly WxH\n");
    v_printf(2,"   -b, --breakpoints          report breakpoints (truncation offsets) for truncations at scales 1:8, 1:4, 1:2\n");
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
    v_printf(3," (___ | | | ___)   ");v_printf(2,"FLIF (Free Lossless Image Format) 0.3 [28 April 2017]\n");
    v_printf(3,"  (__ | |_| __)    ");v_printf(3,"Copyright (C) 2017 Jon Sneyers and Pieter Wuille\n");
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
bool check_metadata_extension (char *ext) {
    if (!(ext && (
                   !strcasecmp(ext,".icc")
                || !strcasecmp(ext,".xmp")
                || !strcasecmp(ext,".exif")
                  ))) {
        return false;
    } else {
        return true;
    }
}


#ifdef HAS_ENCODER

bool encode_load_input_images(int argc, char **argv, Images &images, flif_options &options) {
    int nb_input_images = argc-1;
    int nb_actual_images = 0;
    metadata_options md;
    md.icc = options.color_profile;
    md.xmp = options.metadata;
    md.exif = options.metadata;
    while(argc>1) {
      int maxlength = strlen(argv[0])+100;
      std::vector<char> vfilename(maxlength);
      char *filename = argv[0];
      int framecounter = 0;
      int stop_searching = 0;
      bool multiple=false;
      if (!file_exists(argv[0]) && strchr(argv[0],'%')) {
        multiple=true;
        filename = &vfilename[0];
      }
      for ( ; framecounter < 0xFFFFFFF && stop_searching < 1000; framecounter++ ) {
        if (multiple) {
             snprintf(filename,maxlength,argv[0],framecounter);
             if (!file_exists(filename)) {
                stop_searching++;
                continue;
             }
             stop_searching = 0;
        }
        Image image;
        if (options.keep_palette) image.palette = true; // tells the PNG loader to keep palette intact
        v_printf_tty(2,"\r");
        if (!image.load(filename,md)) {
            e_printf("Could not read input file: %s\n", argv[0]);
            return false;
        };
        if (image.numPlanes() > 0) {
          nb_actual_images++;
          images.push_back(std::move(image));
          Image& last_image = images.back();
          if (last_image.rows() != images[0].rows() || last_image.cols() != images[0].cols()) {
            e_printf("Dimensions of all input images should be the same!\n");
            e_printf("  First image is %ux%u\n",images[0].cols(),images[0].rows());
            e_printf("  This image is %ux%u: %s\n",last_image.cols(),last_image.rows(),filename);
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
        } else {
          if (images.size() == 0) {
            e_printf("First specify the actual image(s), then the metadata.\n");
            return false;
          }
          for (size_t i=0; i<image.metadata.size(); i++)
              images[0].metadata.push_back(image.metadata[i]);
        }
        if (nb_input_images>1) {v_printf(2,"    (%i/%i)         ",(int)images.size(),nb_input_images); v_printf(4,"\n");}
        if (!multiple) break;
      }
      argc--; argv++;
    }
    if (nb_actual_images > 0) return true;
    e_printf("Error: no actual input images to be encoded!\n");
    return false;
}
bool encode_flif(int argc, char **argv, Images &images, flif_options &options) {
    bool flat=true;
    unsigned int framenb=0;
    for (Image& i : images) { i.frame_delay = options.frame_delay[framenb]; if (framenb+1 < options.frame_delay.size()) framenb++; }
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
      if (options.plc && (images[0].getDepth() > 8 || !options.loss)) {
        desc.push_back("Channel_Compact");  // compactify channels (not if lossy, because then loss gets magnified!)
      }
      if (options.ycocg) {
        desc.push_back("YCoCg");  // convert RGB(A) to YCoCg(A)
      }
      desc.push_back("PermutePlanes");  // permute RGB to GRB
      desc.push_back("Bounds");  // get the bounds of the color spaces
    }
    // only use palette/CB if we're lossless, because lossy and palette don't go well together...
    if (options.palette_size == -1) {
        options.palette_size = DEFAULT_MAX_PALETTE_SIZE;
        if (nb_pixels * images.size() / 3 < DEFAULT_MAX_PALETTE_SIZE) {
          options.palette_size = nb_pixels * images.size() / 3;
        }
    }
    if (!options.loss && options.palette_size != 0) {
        desc.push_back("Palette_Alpha");  // try palette (including alpha)
        desc.push_back("Palette");  // try palette (without alpha)
    }
    if (!options.loss) {
    if (options.acb == -1) {
      // not specified if ACB should be used
      if (nb_pixels * images.size() > 10000) {
        desc.push_back("Color_Buckets");  // try auto color buckets on large images
      }
    } else if (options.acb) {
      desc.push_back("Color_Buckets");  // try auto color buckets if forced
    }
    }
    if (options.method.o == Optional::undefined) {
        // no method specified, pick one heuristically
        if (nb_pixels * images.size() < 10000) options.method.encoding=flifEncoding::nonInterlaced; // if the image is small, not much point in doing interlacing
        else options.method.encoding=flifEncoding::interlaced; // default method: interlacing
    }
    if (images.size() > 1) {
        desc.push_back("Duplicate_Frame");  // find duplicate frames
        if (!options.loss) { // only if lossless
          if (options.frs) desc.push_back("Frame_Shape");  // get the shapes of the frames
          if (options.lookback) desc.push_back("Frame_Lookback");  // make a "deep" alpha channel (negative values are transparent to some previous frame)
        }
    }
    if (options.learn_repeats < 0) {
        // no number of repeats specified, pick a number heuristically
        options.learn_repeats = TREE_LEARN_REPEATS;
        //if (nb_pixels * images.size() < 5000) learn_repeats--;        // avoid large trees for small images
        if (options.learn_repeats < 0) options.learn_repeats=0;
    }
    bool result = true;
    if (!options.just_add_loss) {
      FILE *file = NULL;
      if (!strcmp(argv[0],"-")) file = stdout;
      else file = fopen(argv[0],"wb");
      if (!file) return false;
      FileIO fio(file, (file == stdout? "to standard output" : argv[0]));
      if (!flif_encode(fio, images, desc, options)) result = false;
    } else {
      BlobIO bio; // will just contain some unneeded FLIF header stuff
      if (!flif_encode(bio, images, desc, options)) result = false;
      else if (!images[0].save(argv[0])) result = false;
    }
    // get rid of palette
    images[0].clear();
    return result;
}

bool handle_encode(int argc, char **argv, Images &images, flif_options &options) {
    if (!encode_load_input_images(argc,argv,images,options)) return false;
    if (!options.alpha_zero_special) for (Image& i : images) i.alpha_zero_special = false;
    argv += (argc-1);
    argc = 1;
    return encode_flif(argc, argv, images, options);
}
#endif

bool decode_flif(char **argv, Images &images, flif_options &options) {
    FILE *file = NULL;
    if (!strcmp(argv[0],"-")) file = stdin;
    else file = fopen(argv[0],"rb");
    if(!file) return false;
    FileIO fio(file, (file==stdin ? "from standard input" : argv[0]));
    metadata_options md;
    md.icc = options.color_profile;
    md.xmp = options.metadata;
    md.exif = options.metadata;
    return flif_decode(fio, images, options, md);
}

int handle_decode(int argc, char **argv, Images &images, flif_options &options) {
    if (options.scale < 0) {
        // just identify the file(s), don't actually decode
        while (argc>0) {
            decode_flif(argv, images, options);
            argv++; argc--;
        }
        return 0;
    }
    if (argc == 1 && options.show_breakpoints) {
        decode_flif(argv, images, options);
        return 0;
    }
    char *ext = strrchr(argv[1],'.');
    if (check_metadata_extension(ext)) {
        // only requesting metadata, no need to actually decode the file
        options.scale = -2;
        decode_flif(argv, images, options);
        if (!images[0].save(argv[1])) return 2;
        v_printf(2,"\n");
        return 0;
    }
    if (!check_compatible_extension(ext) && strcmp(argv[1],"null:") && strcmp(argv[1],"-")) {
        e_printf("Error: expected \".png\", \".pnm\" or \".pam\" file name extension for output file\n");
        return 1;
    }
    if (!(ext && ( !strcasecmp(ext,".png"))))
        options.keep_palette = false;   // don't try to make a palette PNM

    try {
      if (!decode_flif(argv, images, options)) {
        e_printf("Error: could not decode FLIF file\n"); return 3;
      }
    } catch (std::bad_alloc& ba) {
        e_printf("Error: memory allocation problem (unexpected)\n"); return 3;
    }
    if (!strcmp(argv[1],"null:")) return 0;
//    if (scale>1)
//        v_printf(3,"Downscaling output: %ux%u -> %ux%u\n",images[0].cols(),images[0].rows(),images[0].cols()/scale,images[0].rows()/scale);
    if (images.size() == 1) {
        if (!images[0].save(argv[1])) return 2;
    } else {
        bool to_stdout=false;
        if (!strcmp(argv[1],"-")) {
            to_stdout=true;
            v_printf(1,"Warning: writing animation to standard output as a concatenation of PAM files.\n");
        }
        int counter=0;
        int maxlength = strlen(argv[1])+100;
        std::vector<char> vfilename(maxlength);
        char *filename = &vfilename[0];
        bool use_custom_format = false;
        if (strchr(argv[1],'%')) use_custom_format = true;
        strcpy(filename,argv[1]);
        char *a_ext = strrchr(filename,'.');
        if (!a_ext && !to_stdout) {
            e_printf("Problem saving animation to %s\n",filename);
            return 2;
        }
        for (Image& image : images) {
            if (!to_stdout) {
              if (use_custom_format) snprintf(filename,maxlength,argv[1],counter);
              else if (images.size() < 1000) sprintf(a_ext,"-%03d%s",counter,ext);
              else if (images.size() < 10000) sprintf(a_ext,"-%04d%s",counter,ext);
              else if (images.size() < 100000) sprintf(a_ext,"-%05d%s",counter,ext);
              else sprintf(a_ext,"-%08d%s",counter,ext);
              if (file_exists(filename) && !options.overwrite) {
                e_printf("Error: output file already exists: %s\nUse --overwrite to force overwrite.\n",filename);
                return 4;
              }
              if (!image.save(filename)) return 2;
            } else {
              if (!image.save(argv[1])) return 2;
            }
            counter++;
            v_printf(2,"    (%i/%i)         \r",counter,(int)images.size()); v_printf(4,"\n");
        }
    }
    // get rid of palette (should also do this in the non-standard/error paths, but being lazy here since the tool will exit anyway)
    images[0].clear();
    v_printf(2,"\n");
    return 0;
}
int main(int argc, char **argv)
{
    Images images;
    flif_options options = FLIF_DEFAULT_OPTIONS;
#ifdef HAS_ENCODER
    int mode = -1; // 0 = encode, 1 = decode, 2 = transcode
#else
    int mode = 1;
#endif
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
        {"no-metadata", 0, NULL, 'm'},
        {"no-color-profile", 0, NULL, 'p'},
        {"quality", 1, NULL, 'q'},
        {"scale", 1, NULL, 's'},
        {"resize", 1, NULL, 'r'},
        {"fit", 1, NULL, 'f'},
        {"identify", 0, NULL, 'i'},
        {"version", 0, NULL, 'V'},
        {"overwrite", 0, NULL, 'o'},
        {"breakpoints", 0, NULL, 'b'},
        {"keep-palette", 0, NULL, 'k'},
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
        {"chroma-subsample", 0, NULL, 'J'},
        {"no-subtract-green", 0, NULL, 'W'},
#endif
        {0, 0, 0, 0}
    };
    int i,c;
#ifdef HAS_ENCODER
    while ((c = getopt_long (argc, argv, "hdvcmiVq:s:r:f:obketINnF:KP:ABYWCL:SR:D:M:T:X:Z:Q:UG:H:E:J", optlist, &i)) != -1) {
#else
    while ((c = getopt_long (argc, argv, "hdvcmiVq:s:r:f:obk", optlist, &i)) != -1) {
#endif
        switch (c) {
        case 'd': mode=1; break;
        case 'v': increase_verbosity(); break;
        case 'V': increase_verbosity(3); break;
        case 'c': options.crc_check = 0; break;
        case 'm': options.metadata = 0; break;
        case 'p': options.color_profile = 0; break;
        case 'o': options.overwrite = 1; break;
        case 'q': options.quality=atoi(optarg);
                  if (options.quality < 0 || options.quality > 100) {e_printf("Not a sensible number for option -q\n"); return 1; }
                  break;
        case 's': options.scale=atoi(optarg);
                  if (options.scale < 1 || options.scale > 128) {e_printf("Not a sensible number for option -s\n"); return 1; }
                  break;
        case 'r': if (sscanf(optarg,"%ix%i", &options.resize_width, &options.resize_height) < 1) {
                    if (sscanf(optarg,"x%i", &options.resize_height) < 1) {e_printf("Not a sensible value for option -r (expected WxH)\n"); return 1; }
                  }
                  if (!options.resize_height) options.resize_height = options.resize_width;
                  break;
        case 'f': if (sscanf(optarg,"%ix%i", &options.resize_width, &options.resize_height) < 1) {
                    if (sscanf(optarg,"x%i", &options.resize_height) < 1) {e_printf("Not a sensible value for option -f (expected WxH)\n"); return 1; }
                  }
                  options.fit=1;
                  break;
        case 'i': options.scale = -1; break;
        case 'b': options.show_breakpoints = 8; mode=1; break;
        case 'k': options.keep_palette = true; break;
#ifdef HAS_ENCODER
        case 'e': mode=0; break;
        case 't': mode=2; break;
        case 'I': options.method.encoding=flifEncoding::interlaced; break;
        case 'n': // undocumented: lower case -n still works
        case 'N': options.method.encoding=flifEncoding::nonInterlaced; break;
        case 'A': options.acb=1; break;
        case 'B': options.acb=0; break;
        case 'P': options.palette_size=atoi(optarg);
                  if (options.palette_size < -32000 || options.palette_size > 32000) {e_printf("Not a sensible number for option -P\n"); return 1; }
                  if (options.palette_size > 512) {v_printf(1,"Warning: palette size above 512 implies that simple FLIF decoders (8-bit only) cannot decode this file.\n"); }
                  if (options.palette_size == 0) {v_printf(5,"Palette disabled\n"); }
                  break;
        case 'R': options.learn_repeats=atoi(optarg);
                  if (options.learn_repeats < 0 || options.learn_repeats > 20) {e_printf("Not a sensible number for option -R\n"); return 1; }
                  break;
        case 'F': options.frame_delay.clear();
                  while(optarg != 0) {
                    int d=strtol(optarg,&optarg,10);
                    if (d==0) break;
                    if (*optarg == ',' || *optarg == '+' || *optarg == ' ') optarg++;
                    options.frame_delay.push_back(d);
                    if (d < 0 || d > 60000) {e_printf("Not a sensible number for option -F: %i\n",d); return 1; }
                  }
                  if (options.frame_delay.size() < 1) options.frame_delay.push_back(100);
                  break;
        case 'L': options.lookback=atoi(optarg);
                  if (options.lookback < -1 || options.lookback > 256) {e_printf("Not a sensible number for option -L\n"); return 1; }
                  break;
        case 'D': options.divisor=atoi(optarg);
                  if (options.divisor <= 0 || options.divisor > 0xFFFFFFF) {e_printf("Not a sensible number for option -D\n"); return 1; }
                  break;
        case 'M': options.min_size=atoi(optarg);
                  if (options.min_size < 0) {e_printf("Not a sensible number for option -M\n"); return 1; }
                  break;
        case 'T': options.split_threshold=atoi(optarg);
                  if (options.split_threshold <= 3 || options.split_threshold > 100000) {e_printf("Not a sensible number for option -T\n"); return 1; }
                  options.split_threshold *= 5461;
                  break;
        case 'Y': options.ycocg=0; break;
        case 'W': options.ycocg=0; options.subtract_green=0; break;
        case 'C': options.plc=0; break;
        case 'S': options.frs=0; break;
        case 'K': options.alpha_zero_special=0; break;
        case 'X': options.cutoff=atoi(optarg);
                  if (options.cutoff < 1 || options.cutoff > 128) {e_printf("Not a sensible number for option -X (try something between 1 and 128)\n"); return 1; }
                  break;
        case 'Z': options.alpha=atoi(optarg);
                  if (options.alpha < 2 || options.alpha > 128) {e_printf("Not a sensible number for option -Z (try something between 2 and 128)\n"); return 1; }
                  break;
        case 'Q': options.loss=100-atoi(optarg);
                  // can't go above quality 100 = lossless
                  // can go way below 0 if you want
                  if (options.loss < 0) {e_printf("Not a sensible number for option -Q (try something between 0 and 100)\n"); return 1; }
                  break;
        case 'U': options.adaptive=1; break;
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
                        case '3':
                        case 'X': case 'x': // auto/mixed, usually a bad idea
                            d = -1; break;
                        case ' ': case ',': case '+':
                            optarg++; continue;
                        default:
                        e_printf("Not a sensible value for option -G\nValid values are: 0 (avg), 1 (median avg/gradients), 2 (median neighbors), X (auto/mixed), ? (heuristically pick 0-2)\n"); return 1;
                    }
                    if (p>4) {e_printf("Error while parsing option -G: too many planes specified\n"); return 1; }
                    options.predictor[p] = d;
                    p++; optarg++;
                  }
                  for (; p<5; p++) options.predictor[p]=options.predictor[0];
                  }
                  break;
        case 'H': options.invisible_predictor=atoi(optarg);
                  if (options.invisible_predictor < 0 || options.invisible_predictor > 2) {e_printf("Not a sensible value for option -H\nValid values are: 0 (avg), 1 (median avg/gradients), 2 (median neighbors)\n"); return 1; }
                  break;
        case 'J': options.chroma_subsampling = 1;
                  break;
        case 'E': {
                  int effort=atoi(optarg);
                  if (effort < 0 || effort > 100) {e_printf("Not a sensible number for option -E (try something between 0 and 100)\n"); return 1; }
                  // set some parameters automatically
                  if (effort < 10) options.learn_repeats=0;
                  else if (effort <= 50) {options.learn_repeats=1; options.split_threshold=5461*8*5;}
                  else if (effort <= 70) {options.learn_repeats=2;  options.split_threshold=5461*8*8;}
                  else if (effort <= 90) {options.learn_repeats=3; options.split_threshold=5461*8*10;}
                  else if (effort <= 100) {options.learn_repeats=4; options.split_threshold=5461*8*12;}
                  if (effort < 15) { for (int i=0; i<5; i++) options.predictor[i]=0; }
                  else if (effort < 30) { options.predictor[1]=0; options.predictor[2]=0; }
                  if (effort < 5) options.acb=0;
                  if (effort < 8) options.palette_size=0;
                  if (effort < 25) options.plc=0;
                  if (effort < 30) options.lookback=0;
                  if (effort < 5) options.frs=0;
                  v_printf(3,"Encode effort: %i, corresponds to parameters -R%i -T%i%s%s%s%s\n", effort, options.learn_repeats, options.split_threshold/5461,
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
    bool last_is_output = (options.scale != -1);
    if (options.show_breakpoints && argc == 1) { last_is_output = false; options.no_full_decode = 1; options.scale = 2; }

    if (!strcmp(argv[argc-1],"-")) {
        // writing output to stdout, so redirecting verbose output to stderr to avoid contaminating the output stream
        redirect_stdout_to_stderr();
    }

    show_banner();
    if (argc == 0 || showhelp) {
        if (get_verbosity() == 1 || showhelp) show_help(mode);
        return 0;
    }

    if (argc == 1 && last_is_output) {
        show_help(mode);
        e_printf("\nOutput file missing.\n");
        return 1;
    }
    if (options.scale == -1) mode = 1;
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
        if (mode == 0) {
            char *f = strrchr(argv[argc-1],'/');
            char *ext = f ? strrchr(f,'.') : strrchr(argv[argc-1],'.');
            if (ext && !options.loss && strcasecmp(ext,".flif") && strcasecmp(ext,".flf") ) {
                e_printf("Warning: expected file name extension \".flif\" for output file.\n");
            } else if (options.loss && check_compatible_extension(ext)) {
                v_printf(2,"Not doing actual lossy encoding to FLIF, just applying loss.\n");
                options.just_add_loss = 1;
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
        if (!strcmp(argv[0],"-")) {
          v_printf(4,"Taking input from standard input. Mode: %s\n",
             (mode==0?"encode": (mode==1?"decode":"transcode")));
        } else if (!strchr(argv[0],'%')) {
          e_printf("Error: input file does not exist: %s\n",argv[0]);
          return 1;
        }
    }
    if (last_is_output && file_exists(argv[argc-1]) && !options.overwrite) {
        e_printf("Error: output file already exists: %s\nUse --overwrite to force overwrite.\n",argv[argc-1]);
        return 1;
    }
    if (mode > 0 && argc > 2 && options.scale != -1) {
        e_printf("Too many arguments.\n");
        return 1;
    }

#ifdef HAS_ENCODER
    if (options.chroma_subsampling)
        v_printf(1,"Warning: chroma subsampling produces a truncated FLIF file. Image will not be lossless!\n");
    if (options.loss > 0) options.keep_palette = false; // not going to add loss to indexed colors
    if (options.adaptive) options.loss = -options.loss; // use negative loss to indicate we want to do adaptive lossy encoding
    if (mode == 0) {
        if (!handle_encode(argc, argv, images, options)) return 2;
    } else if (mode == 1) {
#endif
        return handle_decode(argc, argv, images, options);
#ifdef HAS_ENCODER
    } else if (mode == 2) {
//        if (scale > 1) {e_printf("Not yet supported: transcoding downscaled image; use decode + encode!\n");}
        if (!decode_flif(argv, images, options)) return 2;
        argc--; argv++;
        if (!encode_flif(argc, argv, images, options)) return 2;
    }
#endif
    return 0;
}

