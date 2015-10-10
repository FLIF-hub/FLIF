#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "image.h"
#include "image-pam.h"
#include "image-pnm.h"
#include "../common.h"

#define PPMREADBUFLEN 256

bool image_load_pam(const char *filename, Image& image)
{
    FILE *fp = fopen(filename,"rb");
    char buf[PPMREADBUFLEN], *t;

    if (!fp) {
        return false;
    }
    unsigned int width=0,height=0;
    unsigned int maxval=0;
    unsigned int depth = 0;
    t = fgets(buf, PPMREADBUFLEN, fp);
    int type=0;
    if ( (!strncmp(buf, "P7\n", 3)) ) type=7;
    if (type==0) {
        e_printf("PAM file is not of type P7, cannot read other types.\n");
        fclose(fp);
        return false;
    }
    int maxlines=100;
    do {
        t = fgets(buf, PPMREADBUFLEN, fp);
        if ( t == NULL ) return 1;
        /* Px formats can have # comments after first line */
        if (strncmp(buf, "#", 1) == 0 || strncmp(buf, "\n", 1) == 0) continue;
        sscanf(buf, "WIDTH %u\n", &width);
        sscanf(buf, "HEIGHT %u\n", &height);
        sscanf(buf, "DEPTH %u\n", &depth);
        sscanf(buf, "MAXVAL %u\n", &maxval);
        if (maxlines-- < 1) {e_printf("Problem while parsing PAM header.\n"); fclose(fp); return false;}
    } while ( strncmp(buf, "ENDHDR", 6) != 0 );
    if (depth>4 || depth <1 || width <1 || height < 1 || maxval<1 || maxval > 0xffff) {
        e_printf("Couldn't parse PAM header, or unsupported kind of PAM file.\n");
        fclose(fp);
        return false;
    }


    unsigned int nbplanes=depth;
    image.init(width, height, 0, maxval, nbplanes);
      if (maxval > 0xff) {
        for (unsigned int y=0; y<height; y++) {
          for (unsigned int x=0; x<width; x++) {
            for (unsigned int c=0; c<nbplanes; c++) {
                ColorVal pixel= (fgetc(fp) << 8);
                pixel += fgetc(fp);
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

    fclose(fp);
    return true;
}

bool image_save_pam(const char *filename, const Image& image)
{
    if (image.numPlanes() < 4) return image_save_pnm(filename, image);
    FILE *fp = fopen(filename,"wb");
    if (!fp) {
        return false;
    }

        ColorVal max = image.max(0);

        if (max > 0xffff) {
            e_printf("Cannot store as PAM. Find out why.\n");
            fclose(fp);
            return false;
        }
        unsigned int height = image.rows(), width = image.cols();
        fprintf(fp,"P7\nWIDTH %u\nHEIGHT %u\nDEPTH 4\nMAXVAL %i\nTUPLTYPE RGB_ALPHA\nENDHDR\n", width, height, max);


// experiment: output RGBA pixels in FLIF's Adam-infinity interlaced order
/*
                fputc(image(0,0,0) & 0xFF,fp);
                fputc(image(1,0,0) & 0xFF,fp);
                fputc(image(2,0,0) & 0xFF,fp);
                fputc(image(3,0,0) & 0xFF,fp);

    for (int z = image.zooms(); z >= 0; z--) {
      if (z % 2 == 0) {
          for (uint32_t y = 1; y < image.rows(z); y += 2) {
            for (uint32_t x = 0; x < image.cols(z); x++) {
                if (max > 0xff) fputc(image(0,z,y,x) >> 8,fp);
                fputc(image(0,z,y,x) & 0xFF,fp);
                if (max > 0xff) fputc(image(1,z,y,x) >> 8,fp);
                fputc(image(1,z,y,x) & 0xFF,fp);
                if (max > 0xff) fputc(image(2,z,y,x) >> 8,fp);
                fputc(image(2,z,y,x) & 0xFF,fp);
                if (max > 0xff) fputc(image(3,z,y,x) >> 8,fp);
                fputc(image(3,z,y,x) & 0xFF,fp);
            }
          }
      } else {
          for (uint32_t y = 0; y < image.rows(z); y++) {
            for (uint32_t x = 1; x < image.cols(z); x += 2) {
                if (max > 0xff) fputc(image(0,z,y,x) >> 8,fp);
                fputc(image(0,z,y,x) & 0xFF,fp);
                if (max > 0xff) fputc(image(1,z,y,x) >> 8,fp);
                fputc(image(1,z,y,x) & 0xFF,fp);
                if (max > 0xff) fputc(image(2,z,y,x) >> 8,fp);
                fputc(image(2,z,y,x) & 0xFF,fp);
                if (max > 0xff) fputc(image(3,z,y,x) >> 8,fp);
                fputc(image(3,z,y,x) & 0xFF,fp);
            }
          }
      }
    }
*/


        for (unsigned int y = 0; y < height; y++) {
            for (unsigned int x = 0; x < width; x++) {
                if (max > 0xff) fputc(image(0,y,x) >> 8,fp);
                fputc(image(0,y,x) & 0xFF,fp);
                if (max > 0xff) fputc(image(1,y,x) >> 8,fp);
                fputc(image(1,y,x) & 0xFF,fp);
                if (max > 0xff) fputc(image(2,y,x) >> 8,fp);
                fputc(image(2,y,x) & 0xFF,fp);
                if (max > 0xff) fputc(image(3,y,x) >> 8,fp);
                fputc(image(3,y,x) & 0xFF,fp);
            }
        }

    fclose(fp);
    return true;

}
