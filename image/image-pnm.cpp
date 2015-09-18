#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "image.h"
#include "image-pnm.h"
#include "../flif.h"

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
    if (type==0) {
        fprintf(stderr,"PPM file is not of type P4, P5 or P6, cannot read other types.\n");
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
    if ( (r < 2) || maxval<1 || maxval>255 ) {
        fprintf(stderr,"Invalid PPM file.\n");
        fclose(fp);
        return 2;
    }
    } else maxval=1;
    unsigned int nbplanes=(type==6?3:1);
    image.init(width, height, 0, maxval, nbplanes);
    if (type==4) {
      char byte=0;
      for (unsigned int y=0; y<height; y++) {
        for (unsigned int x=0; x<width; x++) {
                if (x%8 == 0) byte = fgetc(fp);
                image(0,y,x) = (byte & (128>>(x%8)) ? 0 : 1);
        }
      }
    } else {
      for (unsigned int y=0; y<height; y++) {
        for (unsigned int x=0; x<width; x++) {
            for (unsigned int c=0; c<nbplanes; c++) {
                image(c,y,x) = fgetc(fp);
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
        if (image.numPlanes() == 4) v_printf(1,"WARNING: image has alpha channel, saving to flat PPM! Save to .PNG if you want to keep the alpha channel!\n");
        ColorVal max = std::max(std::max(image.max(0), image.max(1)), image.max(2));
        ColorVal min = std::min(std::min(image.min(0), image.min(1)), image.min(2));

        if (max-min > 255) {
            fprintf(stderr,"Cannot store as PPM. Find out why.\n");
            fclose(fp);
            return false;
        }

        unsigned int height = image.rows(), width = image.cols();
        fprintf(fp,"P6\n%u %u\n%i\n", width, height, max-min);
        for (unsigned int y = 0; y < height; y++) {
            for (unsigned int x = 0; x < width; x++) {
                fputc((image(0,y,x) - min) & 0xFF,fp);
                fputc((image(1,y,x) - min) & 0xFF,fp);
                fputc((image(2,y,x) - min) & 0xFF,fp);
            }
        }
    } else if (image.numPlanes() == 1) {
        ColorVal max = image.max(0);
        ColorVal min = image.min(0);

        if (max-min > 255) {
            fprintf(stderr,"Cannot store as PPM. Find out why.\n");
            fclose(fp);
            return false;
        }

        unsigned int height = image.rows(), width = image.cols();
        fprintf(fp,"P5\n%u %u\n%i\n", width, height, max-min);
        for (unsigned int y = 0; y < height; y++) {
            for (unsigned int x = 0; x < width; x++) {
                fputc((image(0,y,x) - min) & 0xFF,fp);
            }
        }
    } else {
        fprintf(stderr,"Cannot store as PPM. Find out why.\n");
        fclose(fp);
        return false;
    }
    fclose(fp);
    return true;

}
