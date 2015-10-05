#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "image.h"
#include "image-pnm.h"
#include "image-pam.h"
#include "../common.h"

#define PPMREADBUFLEN 256

bool image_load_pnm(const char *filename, Image& image)
{
    FILE *fp = fopen(filename,"rb");
    char buf[PPMREADBUFLEN], *t;
    int r;
    if (!fp) {
        return false;
    }
    unsigned int width=0,height=0;
    unsigned int maxval=0;
    t = fgets(buf, PPMREADBUFLEN, fp);
    int type=0;
    if ( (!strncmp(buf, "P4\n", 3)) ) type=4;
    if ( (!strncmp(buf, "P5\n", 3)) ) type=5;
    if ( (!strncmp(buf, "P6\n", 3)) ) type=6;
    if ( (!strncmp(buf, "P7\n", 3)) ) {fclose(fp); image_load_pam(filename, image);}
    if (type==0) {
        fprintf(stderr,"PNM file is not of type P4, P5 or P6, cannot read other types.\n");
        fclose(fp);
        return false;
    }
    do {
        /* Px formats can have # comments after first line */
        t = fgets(buf, PPMREADBUFLEN, fp);
        if ( t == NULL ) return 1;
    } while ( strncmp(buf, "#", 1) == 0 || strncmp(buf, "\n", 1) == 0);
    r = sscanf(buf, "%u %u", &width, &height);
    if ( r < 2 ) {
        fclose(fp);
        return false;
    }

    if (type>4) {
    char bla;
    r = fscanf(fp, "%u%c", &maxval, &bla);
    if ( (r < 2) || maxval<1 || maxval > 0xffff ) {
        fprintf(stderr,"Invalid PNM file.\n");
        fclose(fp);
        return false;
    }
    } else maxval=1;
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
    fclose(fp);
    return true;
}

bool image_save_pnm(const char *filename, const Image& image)
{
    FILE *fp = fopen(filename,"wb");
    if (!fp) {
        return false;
    }

    if (image.numPlanes() >= 3) {
        if (image.numPlanes() == 4 && image.uses_alpha())
            v_printf(1,"WARNING: image has alpha channel, saving to flat PPM! Use .png or .pam if you want to keep the alpha channel!\n");
        ColorVal max = image.max(0);

        if (max > 0xffff) {
            fprintf(stderr,"Cannot store as PNM. Find out why.\n");
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
            fprintf(stderr,"Cannot store as PNM. Find out why.\n");
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
        fprintf(stderr,"Cannot store as PNM. Find out why.\n");
        fclose(fp);
        return false;
    }
    fclose(fp);
    return true;

}
