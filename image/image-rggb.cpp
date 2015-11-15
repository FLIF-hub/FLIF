#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "image.hpp"
#include "image-rggb.hpp"
#include "image-pnm.hpp"
#include "../common.hpp"

#define PPMREADBUFLEN 256

#ifdef HAS_ENCODER
bool image_load_rggb(const char *filename, Image& image)
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
    if ( (!strncmp(buf, "P5\n", 3)) ) type=5;
    if (type==0) {
        e_printf("RGGB file should be a PGM, like the output of \"dcraw -E -4\". Cannot read other types.\n");
        if (fp) {
            fclose(fp);
        }
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
    if ((width&1) || (height&1)) {e_printf("Expected width and height which are multiples of 2\n"); return false;}
    if (type>4) {
    char bla;
    r = fscanf(fp, "%u%c", &maxval, &bla);
    if ( (r < 2) || maxval<1 || maxval > 0xffff ) {
        e_printf("Invalid RGGB file.\n");
        fclose(fp);
        return false;
    }
    } else maxval=1;
#ifndef SUPPORT_HDR
    if (maxval > 0xff) {
        e_printf("RGGB file has more than 8 bit per channel, this FLIF cannot handle that.\n");
        return false;
    }
#endif
    
	unsigned int nbplanes=4;
	// For now, storing G1 pixels in R plane, G2 pixels in G plane, B pixels in B plane and R pixels in Alpha plane could slightly improve compression
	// TODO: Detecting the sensor CFA pattern. This should improve compression of attypic CFA like the fujifilm X e1 sensor
	// IDEAS: (1) Scaling the planes values using the coefficients read in RAW files for YIQ efficiency improvement, (2) Make a new YIQ transform that could handle 2 green planes instead of one
      if (maxval > 0xff) {
	// Initialising 16bpp planes, adjusting the bpp to the image sensor does not improve compression 
	// TODO: (BUG) Alpha plane cleanup should be deactivated. If the image sensor is 16bpp coded, the encoded file might be lossless or broken. 
	maxval=0xFFFF;
	image.init(width/2, height/2, 0, maxval, nbplanes);
        for (unsigned int y=0; y<height; y+=2) {
          for (unsigned int x=0; x<width; x+=2) {
                ColorVal pixel= (fgetc(fp) << 8);
                pixel += fgetc(fp);
                image.set(3,y/2,x/2, 1 + pixel); // R (BUG if 16bpp sensor)
                pixel= (fgetc(fp) << 8);
                pixel += fgetc(fp);
                image.set(0,y/2,x/2, pixel); // G1
          }
          for (unsigned int x=0; x<width; x+=2) {
                ColorVal pixel= (fgetc(fp) << 8);
                pixel += fgetc(fp);
                image.set(1,y/2,x/2, pixel); // G2
                pixel= (fgetc(fp) << 8);
                pixel += fgetc(fp);
                image.set(2,y/2,x/2, pixel); // B
          }
        }
      } else {
	// Initialising 8bpp planes, adjusting the bpp to the image sensor does not improve compression 
	// TODO: (BUG) Alpha plane should be deactivated. If the image sensor is 8bpp coded, the encoded file might be lossless or broken. 
	maxval=0xFF;
	image.init(width/2, height/2, 0, maxval, nbplanes);
        for (unsigned int y=0; y<height; y+=2) {
          for (unsigned int x=0; x<width; x+=2) {
                image.set(3,y/2,x/2, 1 + fgetc(fp)); // R (BUG if 8bpp sensor)
                image.set(0,y/2,x/2, fgetc(fp)); // G1
          }
          for (unsigned int x=0; x<width; x+=2) {
                image.set(1,y/2,x/2, fgetc(fp)); // G2
                image.set(2,y/2,x/2, fgetc(fp)); // B
          }
        }
      }
    fclose(fp);
    return true;
}
#endif

bool image_save_rggb(const char *filename, const Image& image)
{
    if (image.numPlanes() != 4) return false;

    FILE *fp = fopen(filename,"wb");
    if (!fp) {
        return false;
    }

        ColorVal max = image.max(0);

        if (max > 0xffff) {
            e_printf("Cannot store as RGGB. Find out why.\n");
            fclose(fp);
            return false;
        }

        unsigned int height = image.rows(), width = image.cols();
        fprintf(fp,"P5\n%u %u\n%i\n", width*2, height*2, max);
        for (unsigned int y = 0; y < height; y++) {
            for (unsigned int x = 0; x < width; x++) {
                if (max > 0xff) fputc((image(3,y,x)-1) >> 8,fp);  // R
                fputc((image(3,y,x)-1) & 0xFF,fp);
                if (max > 0xff) fputc(image(0,y,x) >> 8,fp);  // G1
                fputc((image(0,y,x)) & 0xFF,fp);
            }
            for (unsigned int x = 0; x < width; x++) {
                if (max > 0xff) fputc(image(1,y,x) >> 8,fp);  // G2
                fputc(image(1,y,x) & 0xFF,fp);
                if (max > 0xff) fputc(image(2,y,x) >> 8,fp);  // B
                fputc(image(2,y,x) & 0xFF,fp);
            }
        }
    fclose(fp);
    return true;

}
