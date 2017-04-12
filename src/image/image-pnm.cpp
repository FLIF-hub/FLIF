#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "image.hpp"
#include "image-pnm.hpp"
#include "image-pam.hpp"

#define PPMREADBUFLEN 256

#ifdef HAS_ENCODER

// auxiliary function to read a non-zero number from a PNM header, ignoring whitespace and comments
unsigned int read_pnm_int(FILE *fp, char* buf, char** t) {
    long result = strtol(*t,t,10);
    if (result == 0) {
        // no valid int found here, try next line
        do {
          *t = fgets(buf, PPMREADBUFLEN, fp);
          if ( *t == NULL ) {return false;}
        } while ( strncmp(buf, "#", 1) == 0 || strncmp(buf, "\n", 1) == 0);
        result = strtol(*t,t,10);
        if (result == 0) { e_printf("Invalid PNM file.\n"); fclose(fp); }
    }
    return result;
}

bool image_load_pnm(const char *filename, Image& image) {
    FILE *fp = NULL;
    if (!strcmp(filename,"-")) fp = fdopen(dup(fileno(stdin)), "rb"); // make sure it is in binary mode (needed in Windows)
    else fp = fopen(filename,"rb");

    char buf[PPMREADBUFLEN], *t;
    if (!fp) {
        e_printf("Could not open file: %s\n", filename);
        return false;
    }
    unsigned int width=0,height=0;
    unsigned int maxval=0;
    do {
        /* # comments before the first line */
        t = fgets(buf, PPMREADBUFLEN, fp);
        if ( t == NULL ) return false;
    } while ( strncmp(buf, "#", 1) == 0 || strncmp(buf, "\n", 1) == 0);
    int type=0;
    if ( (!strncmp(buf, "P4", 2)) ) type=4;
    if ( (!strncmp(buf, "P5", 2)) ) type=5;
    if ( (!strncmp(buf, "P6", 2)) ) type=6;
    if ( (!strncmp(buf, "P7", 2)) ) {return image_load_pam_fp(fp, image);}
    if (type==0) {
        if (buf[0] == 'P') e_printf("PNM file is not of type P4, P5, P6 or P7, cannot read other types.\n");
        else e_printf("This does not look like a PNM file.\n");
        fclose(fp);
        return false;
    }
    t += 2;
    if (!(width = read_pnm_int(fp,buf,&t))) return false;
    if (!(height = read_pnm_int(fp,buf,&t))) return false;
    if (type>4) {
      if (!(maxval = read_pnm_int(fp,buf,&t))) return false;
      if ( maxval > 0xffff ) {
        e_printf("Invalid PNM file (more than 16-bit?)\n");
        fclose(fp);
        return false;
      }
    } else maxval=1;
#ifndef SUPPORT_HDR
    if (maxval > 0xff) {
        e_printf("PNM file has more than 8 bit per channel, this FLIF cannot handle that.\n");
        return false;
    }
#endif
    unsigned int nbplanes=(type==6?3:1);
    image.init(width, height, 0, maxval, nbplanes);
    if (type==4) {
      char byte=0;
      for (unsigned int y=0; y<height; y++) {
        for (unsigned int x=0; x<width; x++) {
                if (x%8 == 0) byte = fgetc(fp);
                image.set(0,y,x, (byte & (128>>(x%8)) ? 0 : 1));
        }
      }
    } else {
      if (maxval > 0xff) {
        for (unsigned int y=0; y<height; y++) {
          for (unsigned int x=0; x<width; x++) {
            for (unsigned int c=0; c<nbplanes; c++) {
                ColorVal pixel= (fgetc(fp) << 8);
                pixel += fgetc(fp);
                if (pixel > (int)maxval) {
                    fclose(fp);
                    e_printf("Invalid PNM file: value %i is larger than declared maxval %u\n", pixel, maxval);
                    return false;
                }
                image.set(c,y,x, pixel);
            }
          }
        }
      } else {
        for (unsigned int y=0; y<height; y++) {
          for (unsigned int x=0; x<width; x++) {
            for (unsigned int c=0; c<nbplanes; c++) {
                image.set(c,y,x, fgetc(fp));
            }
          }
        }
      }
    }
//    if (fp != stdin) fclose(fp);
    return true;
}
#endif

bool image_save_pnm(const char *filename, const Image& image)
{
    FILE *fp = NULL;
    if (!strcmp(filename,"-")) fp = fdopen(dup(fileno(stdout)), "wb"); // make sure it is in binary mode (needed in Windows)
    else fp = fopen(filename,"wb");
    if (!fp) {
        return false;
    }

    if (image.numPlanes() >= 3) {
        if (image.numPlanes() == 4 && image.uses_alpha())
            v_printf(1,"WARNING: image has alpha channel, saving to flat PPM! Use .png or .pam if you want to keep the alpha channel!\n");
        ColorVal max = image.max(0);

        if (max > 0xffff) {
            e_printf("Cannot store as PNM. Find out why.\n");
            fclose(fp);
            return false;
        }

        unsigned int height = image.rows(), width = image.cols();
        fprintf(fp,"P6\n%u %u\n%i\n", width, height, max);
        for (unsigned int y = 0; y < height; y++) {
            for (unsigned int x = 0; x < width; x++) {
                if (max > 0xff) fputc(image(0,y,x) >> 8,fp);
                fputc(image(0,y,x) & 0xFF,fp);
                if (max > 0xff) fputc(image(1,y,x) >> 8,fp);
                fputc(image(1,y,x) & 0xFF,fp);
                if (max > 0xff) fputc(image(2,y,x) >> 8,fp);
                fputc(image(2,y,x) & 0xFF,fp);
            }
        }
    } else if (image.numPlanes() == 1) {
        ColorVal max = image.max(0);

        if (max > 0xffff) {
            e_printf("Cannot store as PNM. Find out why.\n");
            fclose(fp);
            return false;
        }

        unsigned int height = image.rows(), width = image.cols();
        fprintf(fp,"P5\n%u %u\n%i\n", width, height, max);
        for (unsigned int y = 0; y < height; y++) {
            for (unsigned int x = 0; x < width; x++) {
                if (max > 0xff) fputc(image(0,y,x) >> 8,fp);
                fputc(image(0,y,x) & 0xFF,fp);
            }
        }
    } else {
        e_printf("Cannot store as PNM. Find out why.\n");
        fclose(fp);
        return false;
    }
//    if (fp != stdout) fclose(fp);
    if (image.get_metadata("iCCP")) {
        v_printf(1,"Warning: input image has color profile, which cannot be stored in output image format.\n");
    }
    fclose(fp);
    return true;

}
