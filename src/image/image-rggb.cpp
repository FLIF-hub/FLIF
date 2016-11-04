#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "image.hpp"
#include "image-rggb.hpp"
#include "image-pnm.hpp"
#include "image-pam.hpp"
#include "image-png.hpp"

#define PPMREADBUFLEN 256

#ifdef HAS_ENCODER
bool image_load_rggb(const char *filename, Image& image, metadata_options &md)
{
	FILE *fp = fopen(filename,"rb");
	char buf[PPMREADBUFLEN];
	int r;
	if (!fp) {
		return false;
	}
	unsigned int width=0,height=0;
	unsigned int maxval=0;
	unsigned int idx_comments=0,idx_magic=0;
	char comments[PPMREADBUFLEN],magic[8];
	int type=0;
	// Detect file signature
	magic[idx_magic] = fgetc(fp);
	switch (magic[idx_magic]) {
		case 'P' :	// Look like a PNM
				v_printf(4,"PNM file is detected.\n");
				magic[++idx_magic] = fgetc(fp);
				switch (magic[idx_magic]) {
					case '4':	type=4;
							break;
					case '5':	type=5;
							break;
					case '6':	type=6;
							break;
					case '7':	fclose(fp);
							return image_load_pam(filename, image);
							break;
					default:	e_printf("Unknown magic number.%s\n", magic);
							return false;
							break;
				}
				break;
		case '#' :	// Look like a PNM starting with comments
				v_printf(4,"Comments in PNM file is detected.\n");
				comments[idx_comments++] = magic[idx_magic];
				while (magic[idx_magic]=='#') {
					do {
						comments[idx_comments++] = fgetc(fp);
					} while ( comments[idx_comments -1 ] != char(0x0A));
					magic[idx_magic] = fgetc(fp);
				}
				if (magic[idx_magic]!='P') {
					e_printf("Not a PNM.\n");
					return false;
				}
				magic[++idx_magic] = fgetc(fp);
				switch (magic[idx_magic]) {
					case '4':	type=4;
							break;
					case '5':	type=5;
							break;
					case '6':	type=6;
							break;
					case '7':	fclose(fp);
							return image_load_pam(filename, image);
							break;
					default:	e_printf("Unknown magic number.%s\n", magic);
							return false;
							break;
				}
				break;
		case '\211' :	// Look like a PNG
				v_printf(4,"PNG file is detected.\n");
				do {
						magic[++idx_magic] = fgetc(fp);
				} while ( idx_magic < 8 );
				if ( strcmp(magic, "\211PNG\r\n\032\n") != 0 ) {
					e_printf("Unknown magic number.%s\n", magic);
					return false;
				}
				fclose(fp);
				return !image_load_png(filename, image, md);
				break;
		case 'I' :	// Look like a TIFF/DNG
				e_printf("TODO: Improving TIFF/DNG magic number detection.\n");
				return false;
				break;
		default :	e_printf("Unknown magic number.%s\n", magic);
				return false;
				break;
	}
/*	do {
		// # comments before the first line
		// TODO: Detect CFAPattern comment
		// TODO: Save the comments as metadatas
		t = fgets(buf, PPMREADBUFLEN, fp);
		if ( t == NULL ) return false;
	} while ( strncmp(buf, "#", 1) == 0 || strncmp(buf, "\n", 1) == 0);
	int type=0;
	if ( (!strncmp(buf, "P4", 2)) ) type=4;
	if ( (!strncmp(buf, "P5", 2)) ) type=5;
	if ( (!strncmp(buf, "P6", 2)) ) type=6;
	if ( (!strncmp(buf, "P7", 2)) ) {fclose(fp); return image_load_pam(filename, image);}
*/	if (type==0) {
		e_printf("RGGB file should be a PGM or PNM, like the output of \"dcraw -E -4\". Cannot read other types.\n");
		if (fp) {
			fclose(fp);
		}
		return false;
	}
	// TODO: Save the comments as metadatas
	do {
		/* Px formats can have # comments after first line */
		char *t = fgets(buf, PPMREADBUFLEN, fp);
		if ( t == NULL ) return 1;
		if ( (strcmp(t, "\n") != 0) && (strncmp(buf, "#", 1) == 0)) {
			if (strlen(comments) != 1) strcat(comments, buf);
			else strcpy(comments, buf);
		}
	} while ( strncmp(buf, "#", 1) == 0 || strncmp(buf, "\n", 1) == 0);
	v_printf(4,"Comments:\n%s", comments);
	r = sscanf(buf, "%u %u", &width, &height);
	if ( r < 2 ) {
		fclose(fp);
		return false;
	}
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
	// CFA Detection
	// TODO: Detect the CFAPattern
	bool RGGBmode=0;
	int CFAmode=4;
	if ( strncmp(comments, "# CFAPattern: ", 14) == 0 ) {
		RGGBmode=1;
		if ( strncmp(comments, "# CFAPattern: RGGB", 18) == 0 ) CFAmode=0;
		if ( strncmp(comments, "# CFAPattern: GRBG", 18) == 0 ) CFAmode=1;
		if ( strncmp(comments, "# CFAPattern: BGGR", 18) == 0 ) CFAmode=2;
		if ( strncmp(comments, "# CFAPattern: GBRG", 18) == 0 ) CFAmode=3;
	}
	if (RGGBmode == true) v_printf(2,"RGGBmode detected.\n");
	unsigned int nbplanes;
	if ((type==5) && (RGGBmode=1)) {
		if (((width&1) || (height&1))) {e_printf("Expected width and height which are multiples of 2\n"); return false;}
		nbplanes=4;
		image.init(width/2, height/2, 0, maxval, nbplanes);
	} else {
		nbplanes=(type==6?3:1);
		image.init(width, height, 0, maxval, nbplanes);
	}

	if (type==4) {
		char byte=0;
		for (unsigned int y=0; y<height; y++) {
			for (unsigned int x=0; x<width; x++) {
				if (x%8 == 0) byte = fgetc(fp);
				image.set(0,y,x, (byte & (128>>(x%8)) ? 0 : 1));
			}
		}
	} else if ((type==5) && (RGGBmode=1)) {
		unsigned int Pl0, Pl1, Pl2, Pl3;
		if (CFAmode==0) {		// R G1 G2 B
			Pl0=3;
			Pl1=0;
			Pl2=2;
			Pl3=1;
		} else if (CFAmode==1) {	// G2 R B G1
			Pl0=2;
			Pl1=3;
			Pl2=1;
			Pl3=0;
		} else if (CFAmode==2) {	// B G2 G1 R
			Pl0=1;
			Pl1=2;
			Pl2=0;
			Pl3=3;
		} else if (CFAmode==3) {	// G1 B R G2
			Pl0=0;
			Pl1=1;
			Pl2=3;
			Pl3=2;
		} else {
			e_printf("The CFAmode is unknown.\n");
			return false;
		}
			
		// For now, storing G1 pixels in R plane, G2 pixels in B plane, B pixels in G plane and R pixels in Alpha plane could slightly improve compression
		// TODO: Detecting the sensor CFA pattern. This should improve compression of attypic CFA like the fujifilm X e1 sensor
		// IDEAS: (1) Scaling the planes values using the coefficients read in RAW files for YIQ efficiency improvement,
		//        (2) Make a new YIQ transform that could handle 2 green planes instead of one
		//        (3) Sorting the R, G1, G2, B planes by similarities for YIQ transform, the 2 more similar planes should be at planes 0 and 2, and the third more similar at plane 1
		if (maxval > 0xff) {
			// Initialising 16bpp planes, adjusting the bpp to the image sensor does not improve compression
			maxval=0xFFFF;
			image.init(width/2, height/2, 0, maxval, nbplanes);
			image.alpha_zero_special = false;
			for (unsigned int y=0; y<height; y+=2) {
				for (unsigned int x=0; x<width; x+=2) {
					ColorVal pixel= (fgetc(fp) << 8);
					pixel += fgetc(fp);
					image.set(Pl0,y/2,x/2, pixel); // R
					pixel= (fgetc(fp) << 8);
					pixel += fgetc(fp);
					image.set(Pl1,y/2,x/2, pixel); // G1
				}
				for (unsigned int x=0; x<width; x+=2) {
					ColorVal pixel= (fgetc(fp) << 8);
					pixel += fgetc(fp);
					image.set(Pl2,y/2,x/2, pixel); // G2
					pixel= (fgetc(fp) << 8);
					pixel += fgetc(fp);
					image.set(Pl3,y/2,x/2, pixel); // B
				}
			}
		} else {
			// Initialising 8bpp planes, adjusting the bpp to the image sensor does not improve compression
			maxval=0xFF;
			image.init(width/2, height/2, 0, maxval, nbplanes);
			image.alpha_zero_special = false;
			for (unsigned int y=0; y<height; y+=2) {
				for (unsigned int x=0; x<width; x+=2) {
					image.set(Pl0,y/2,x/2, fgetc(fp)); // R
					image.set(Pl1,y/2,x/2, fgetc(fp)); // G1
				}
				for (unsigned int x=0; x<width; x+=2) {
					image.set(Pl2,y/2,x/2, fgetc(fp)); // G2
					image.set(Pl3,y/2,x/2, fgetc(fp)); // B
				}
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
	//TODO: Use the metadatas for getting the right CFA pattern
	unsigned int height = image.rows(), width = image.cols();
	fprintf(fp,"P5\n%u %u\n%i\n", width*2, height*2, max);
	for (unsigned int y = 0; y < height; y++) {
		for (unsigned int x = 0; x < width; x++) {
			if (max > 0xff) fputc(image(3,y,x) >> 8,fp);  // R
			fputc(image(3,y,x) & 0xFF,fp);
			if (max > 0xff) fputc(image(0,y,x) >> 8,fp);  // G1
			fputc(image(0,y,x) & 0xFF,fp);
		}
		for (unsigned int x = 0; x < width; x++) {
			if (max > 0xff) fputc(image(2,y,x) >> 8,fp);  // G2
			fputc(image(2,y,x) & 0xFF,fp);
			if (max > 0xff) fputc(image(1,y,x) >> 8,fp);  // B
			fputc(image(1,y,x) & 0xFF,fp);
		}
	}
	fclose(fp);
	return true;
}
