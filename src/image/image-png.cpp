#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef FLIF_USE_STB_IMAGE
#define STB_IMAGE_IMPLEMENTATION
#include "../../extern/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../extern/stb_image_write.h"
enum {
  PNG_COLOR_TYPE_GRAY = 1,
  PNG_COLOR_TYPE_GRAY_ALPHA = 2,
  PNG_COLOR_TYPE_RGB = 3,
  PNG_COLOR_TYPE_RGB_ALPHA = 4,
};
#else
#include <png.h>
#include <zlib.h>
#endif

#include <vector>

#include "image.hpp"
#include "image-png.hpp"

#ifdef HAS_ENCODER
#include "image-png-metadata.hpp"


int image_load_png(const char *filename, Image &image, metadata_options &options) {
#ifdef FLIF_USE_STB_IMAGE

  int x,y,n;
  unsigned char *data = stbi_load(filename, &x, &y, &n, 4);
  if(!data) {
    return 1;
  }

  size_t width = x;
  size_t height = y;
  int bit_depth = 8;
  int color_type = n;

  unsigned int nbplanes;
  if (color_type == PNG_COLOR_TYPE_GRAY) nbplanes=1;
  else if (color_type == PNG_COLOR_TYPE_RGB) nbplanes=3;
  else if (color_type == PNG_COLOR_TYPE_RGB_ALPHA || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) nbplanes=4;
  else { e_printf("Unsupported PNG color type\n"); return 5; }
  image.init(width, height, 0, (1 << bit_depth) - 1, nbplanes);

  for (size_t r = 0; r < height; r++) {
    unsigned char *row = &data[ r * width * 4 ];
    for (size_t c = 0; c < width; c++) {
      image.set(0,r,c, (uint8_t) row[c * 4 + 0]);
      image.set(1,r,c, (uint8_t) row[c * 4 + 1]);
      image.set(2,r,c, (uint8_t) row[c * 4 + 2]);
      image.set(3,r,c, (uint8_t) row[c * 4 + 3]);
    }
  }

  stbi_image_free(data);
  return 0;
#else
  FILE *fp = fopen(filename,"rb");
  if (!fp) {
    return 1;
  }
  png_byte header[8];
  int rr = fread(header,1,8,fp);
  int is_png = !png_sig_cmp(header,0,rr);
  if (!is_png) {
    fclose(fp);
    return 2;
  }
  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,(png_voidp) NULL,NULL,NULL);
  if (!png_ptr) {
    fclose(fp);
    return 3;
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_read_struct(&png_ptr,(png_infopp) NULL,(png_infopp) NULL);
    fclose(fp);
    return 4;
  }

  png_init_io(png_ptr,fp);
  png_set_sig_bytes(png_ptr,8);


  int png_flags = PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND;
  if (image.palette) {
    // allowed to make a palette image
    png_flags = PNG_TRANSFORM_PACKING;
  }
  png_read_png(png_ptr,info_ptr, png_flags, NULL);
//      | PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_STRIP_16

  size_t width = png_get_image_width(png_ptr,info_ptr);
  size_t height = png_get_image_height(png_ptr,info_ptr);
  int bit_depth = png_get_bit_depth(png_ptr,info_ptr);
  int color_type = png_get_color_type(png_ptr,info_ptr);



  unsigned int nbplanes;
  if (color_type == PNG_COLOR_TYPE_GRAY) nbplanes=1;
  else if (color_type == PNG_COLOR_TYPE_RGB) nbplanes=3;
  else if (color_type == PNG_COLOR_TYPE_RGB_ALPHA || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) nbplanes=4;
  else if (color_type == PNG_COLOR_TYPE_PALETTE) nbplanes=4;
  else { e_printf("Unsupported PNG color type\n"); return 5; }
#ifndef SUPPORT_HDR
    if (bit_depth > 8) {
        e_printf("PNG file has more than 8 bit per channel, this FLIF cannot handle that.\n");
        return 6;
    }
#endif
  if (color_type == PNG_COLOR_TYPE_PALETTE) {
      image.semi_init(width, height, 0, (1<<bit_depth)-1, nbplanes);
      image.make_constant_plane(0,0);
      image.make_constant_plane(2,0);
      image.make_constant_plane(3,1);
      image.real_init(true);
      image.palette = true;
      png_colorp palette;
      int nb_colors = 0, nb_alpha = 0;
      png_get_PLTE(png_ptr,info_ptr, &palette,&nb_colors);
      png_bytep trans;
      png_get_tRNS(png_ptr,info_ptr, &trans,&nb_alpha, NULL);
      image.palette_image = new Image(nb_colors,1,0,255,4);
      for (int i=0; i<nb_colors; i++) {
        image.palette_image->set(0,0,i,palette[i].red);
        image.palette_image->set(1,0,i,palette[i].green);
        image.palette_image->set(2,0,i,palette[i].blue);
        if (i < nb_alpha) image.palette_image->set(3,0,i,trans[i]);
        else image.palette_image->set(3,0,i,255);
      }
  } else
      image.init(width, height, 0, (1<<bit_depth)-1, nbplanes);

  png_bytepp rows = png_get_rows(png_ptr,info_ptr);

  if (bit_depth == 8) {
    switch(color_type) {
        case PNG_COLOR_TYPE_PALETTE:
          for (size_t r = 0; r < height; r++) {
            png_bytep row = rows[r];
            for (size_t c = 0; c < width; c++) {
              image.set(1,r,c, (uint8_t) row[c * 1 + 0]);
            }
          }
          break;
        case PNG_COLOR_TYPE_GRAY:
          for (size_t r = 0; r < height; r++) {
            png_bytep row = rows[r];
            for (size_t c = 0; c < width; c++) {
              image.set(0,r,c, (uint8_t) row[c * 1 + 0]);
            }
          }
          break;
        case PNG_COLOR_TYPE_GRAY_ALPHA:
          for (size_t r = 0; r < height; r++) {
            png_bytep row = rows[r];
            for (size_t c = 0; c < width; c++) {
              image.set(0,r,c, (uint8_t) row[c * 2 + 0]);
              image.set(1,r,c, (uint8_t) row[c * 2 + 0]);
              image.set(2,r,c, (uint8_t) row[c * 2 + 0]);
              image.set(3,r,c, (uint8_t) row[c * 2 + 1]);
            }
          }
          break;
        case PNG_COLOR_TYPE_RGB:
          for (size_t r = 0; r < height; r++) {
            png_bytep row = rows[r];
            for (size_t c = 0; c < width; c++) {
              image.set(0,r,c, (uint8_t) row[c * 3 + 0]);
              image.set(1,r,c, (uint8_t) row[c * 3 + 1]);
              image.set(2,r,c, (uint8_t) row[c * 3 + 2]);
            }
          }
          break;
        case PNG_COLOR_TYPE_RGB_ALPHA:
          for (size_t r = 0; r < height; r++) {
            png_bytep row = rows[r];
            for (size_t c = 0; c < width; c++) {
              image.set(0,r,c, (uint8_t) row[c * 4 + 0]);
              image.set(1,r,c, (uint8_t) row[c * 4 + 1]);
              image.set(2,r,c, (uint8_t) row[c * 4 + 2]);
              image.set(3,r,c, (uint8_t) row[c * 4 + 3]);
            }
          }
          break;
        default:
          e_printf("Should not happen: unsupported PNG color type!\n");
    }
  } else if (bit_depth == 16) {
    switch(color_type) {
        case PNG_COLOR_TYPE_GRAY:
          for (size_t r = 0; r < height; r++) {
            png_bytep row = rows[r];
            for (size_t c = 0; c < width; c++) {
              image.set(0,r,c, (((uint16_t) row[c * 2 + 0])<<8)+ (uint16_t) row[c * 2 + 1]);
            }
          }
          break;
        case PNG_COLOR_TYPE_GRAY_ALPHA:
          for (size_t r = 0; r < height; r++) {
            png_bytep row = rows[r];
            for (size_t c = 0; c < width; c++) {
              image.set(0,r,c, (((uint16_t) row[c * 4 + 0])<<8)+ (uint16_t) row[c * 4 + 1]);
              image.set(1,r,c, (((uint16_t) row[c * 4 + 0])<<8)+ (uint16_t) row[c * 4 + 1]);
              image.set(2,r,c, (((uint16_t) row[c * 4 + 0])<<8)+ (uint16_t) row[c * 4 + 1]);
              image.set(3,r,c, (((uint16_t) row[c * 4 + 2])<<8)+ (uint16_t) row[c * 4 + 3]);
            }
          }
          break;
        case PNG_COLOR_TYPE_RGB:
          for (size_t r = 0; r < height; r++) {
            png_bytep row = rows[r];
            for (size_t c = 0; c < width; c++) {
              image.set(0,r,c, (((uint16_t) row[c * 6 + 0])<<8)+ (uint16_t) row[c * 6 + 1]);
              image.set(1,r,c, (((uint16_t) row[c * 6 + 2])<<8)+ (uint16_t) row[c * 6 + 3]);
              image.set(2,r,c, (((uint16_t) row[c * 6 + 4])<<8)+ (uint16_t) row[c * 6 + 5]);
            }
          }
          break;
        case PNG_COLOR_TYPE_RGB_ALPHA:
          for (size_t r = 0; r < height; r++) {
            png_bytep row = rows[r];
            for (size_t c = 0; c < width; c++) {
              image.set(0,r,c, (((uint16_t) row[c * 8 + 0])<<8)+ (uint16_t) row[c * 8 + 1]);
              image.set(1,r,c, (((uint16_t) row[c * 8 + 2])<<8)+ (uint16_t) row[c * 8 + 3]);
              image.set(2,r,c, (((uint16_t) row[c * 8 + 4])<<8)+ (uint16_t) row[c * 8 + 5]);
              image.set(3,r,c, (((uint16_t) row[c * 8 + 6])<<8)+ (uint16_t) row[c * 8 + 7]);
            }
          }
          break;
        default:
          e_printf("Should not happen: unsupported PNG color type!\n");
    }
  } else e_printf("Should not happen: unsupported PNG bit depth: %i!\n",bit_depth);

  // look for ICC color profile
  if (options.icc) {
      png_charp name;
      int comp_type;
#if ((PNG_LIBPNG_VER_MAJOR << 8) | PNG_LIBPNG_VER_MINOR << 0) < \
    ((1 << 8) | (5 << 0))
      png_charp profile;
#else  // >= libpng 1.5.0
      png_bytep profile;
#endif
      png_uint_32 len;

      if (png_get_iCCP(png_ptr, info_ptr,
                       &name, &comp_type, &profile, &len) == PNG_INFO_iCCP) {
        image.set_metadata("iCCP", (const unsigned char*)profile, len);
      }
  }
  // look for Exif/XMP metadata
  if (options.exif || options.xmp) {
      png_textp txt = NULL;
      int nb_txt_chunks = png_get_text(png_ptr, info_ptr, &txt, NULL);
      for (int i = 0; i < nb_txt_chunks; i++, txt++) {
        const char * chunkname;
        bool rawprofile = false;
        if (!strcmp(txt->key, "Raw profile type APP1")
         || !strcmp(txt->key, "Raw profile type exif")) {
            if (!options.exif) continue;
            chunkname = "eXif";
            rawprofile = true;
            v_printf(3, "PNG contains Exif metadata, copying it.\n");
        } else if (!strcmp(txt->key, "Raw profile type xmp")) {
            if (!options.xmp) continue;
            chunkname = "eXmp";
            rawprofile = true;
            v_printf(3, "PNG contains XMP metadata, copying it.\n");
        } else if (!strcmp(txt->key, "XML:com.adobe.xmp")) {
            if (!options.xmp) continue;
            chunkname = "eXmp";
            v_printf(3, "PNG contains XMP metadata, copying it.\n");
        } else {
            v_printf(4, "Encountered unknown PNG text chunk (key: %s), ignoring it.\n", txt->key);
            continue;
        }

        png_size_t length;
        switch (txt->compression) {
#ifdef PNG_iTXt_SUPPORTED
            case PNG_ITXT_COMPRESSION_NONE:
            case PNG_ITXT_COMPRESSION_zTXt:
              length = txt->itxt_length;
              break;
#endif
            case PNG_TEXT_COMPRESSION_NONE:
            case PNG_TEXT_COMPRESSION_zTXt:
            default:
              length = txt->text_length;
              break;
        }
        if (rawprofile) {
          unsigned char * buffer = NULL;
          size_t buffer_len = 0;
          if (ProcessRawProfile(txt->text, length, &buffer, &buffer_len)) {
            image.set_metadata(chunkname, buffer, buffer_len);
            free(buffer);
          }
        } else {
          image.set_metadata(chunkname, (const unsigned char*)txt->text, length);
        }

      }
  }


  png_destroy_read_struct(&png_ptr,&info_ptr,(png_infopp) NULL);
  fclose(fp);

  return 0;
#endif
}
#endif

int image_save_png(const char *filename, const Image &image) {

  if (image.cols() > 0x7fffffffL || image.rows() > 0x7fffffffL) {
    e_printf("Image too large to be saved as PNG.\n");
    return 4;
  }

#ifdef FLIF_USE_STB_IMAGE

  int nbplanes = image.numPlanes();
  if (nbplanes == 4 && !image.uses_alpha()) nbplanes=3;
  int bit_depth = 8, bytes_per_value=1;
  if (image.max(0) > 255) {bit_depth = 16; bytes_per_value=2;}

  if (bit_depth != 8) {
    return 1;
  }

  size_t w = image.cols();
  size_t h = image.rows();


  std::vector<unsigned char> data( w * h * nbplanes * bytes_per_value );
  unsigned char *row = data.data();

  for (size_t r = 0; r < h; r++) {
     for (size_t c = 0; c < w; c++) {
        for (int p=0; p<nbplanes; p++) {
           row[(c + r * w) * nbplanes + p] = (unsigned char) (image(p,r,c));
        }
     }
  }

  stbi_write_png( filename, w, h, nbplanes, row, w * nbplanes * bytes_per_value );
  return 0;
#else

  FILE *fp = fopen(filename,"wb");
  if (!fp) {
    return (1);
  }
  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,(png_voidp) NULL,NULL,NULL);
  if (!png_ptr) {
    fclose(fp);
    return (2);
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr,(png_infopp) NULL);
    fclose(fp);
    return (3);
  }

  png_init_io(png_ptr,fp);
  png_set_user_limits(png_ptr, 0x7fffffffL, 0x7fffffffL);
//  png_set_filter(png_ptr,0,PNG_FILTER_PAETH);
//  png_set_compression_level(png_ptr,Z_BEST_COMPRESSION);

  int colortype=PNG_COLOR_TYPE_RGB;
  int nbplanes = image.numPlanes();
  if (nbplanes == 4 && !image.uses_alpha()) nbplanes=3;
  if (nbplanes == 4) colortype=PNG_COLOR_TYPE_RGB_ALPHA;
  if (nbplanes == 1) colortype=PNG_COLOR_TYPE_GRAY;
  int bit_depth = 8, bytes_per_value=1;
  if (image.max(0) > 255) {bit_depth = 16; bytes_per_value=2;}

  bool do_palette=false;

  if (image.palette_image && image.palette_image->cols() > 0 && image.palette_image->cols() <= 256 && bit_depth == 8) {
    do_palette = true;
    colortype = PNG_COLOR_TYPE_PALETTE;
  }

  png_set_IHDR(png_ptr,info_ptr,image.cols(),image.rows(),bit_depth,colortype,PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT,
      PNG_FILTER_TYPE_DEFAULT);

  {
   size_t length;
#if ((PNG_LIBPNG_VER_MAJOR << 8) | PNG_LIBPNG_VER_MINOR << 0) < \
    ((1 << 8) | (5 << 0))
      png_charp profile;
#else  // >= libpng 1.5.0
      png_bytep profile;
#endif
   if (image.get_metadata("iCCP", (unsigned char **) (&profile), &length)) {
    png_set_iCCP(png_ptr, info_ptr,  (png_charp) "icc", 0, profile, length);
    free(profile);
   }
   if (image.get_metadata("eXmp", (unsigned char **) (&profile), &length)) {
#ifdef PNG_iTXt_SUPPORTED
    png_text txt;
    txt.key = (png_charp) "XML:com.adobe.xmp";
    txt.compression = PNG_ITXT_COMPRESSION_zTXt;
    txt.text_length = 0;
    txt.text = (png_charp) profile;
    txt.lang = NULL;
    txt.lang_key = NULL;
    png_set_text(png_ptr, info_ptr, &txt, 1);
#else
    v_printf(1,"Warning: could not write XMP metadata to PNG file because this version of libpng does not support iTXt.\n");
#endif
    free(profile);
   }
  }

  png_color* palette = NULL;
  if (do_palette) {
    unsigned paletteSize = image.palette_image->cols();
    assert(paletteSize <= PNG_MAX_PALETTE_LENGTH);
    palette = (png_color*)png_malloc(png_ptr, paletteSize*sizeof(png_color));
    for (unsigned p = 0; p < paletteSize; p++) {
      png_color* col = &palette[p];
      col->red = image.palette_image->operator()(0,0,p);
      col->green = image.palette_image->operator()(1,0,p);
      col->blue = image.palette_image->operator()(2,0,p);
    }
    png_set_PLTE(png_ptr, info_ptr, palette, paletteSize);
    if (nbplanes > 3) {
      png_byte trans[256];
      for (unsigned p = 0; p < paletteSize; p++) {
        trans[p] = image.palette_image->operator()(3,0,p);
      }
      png_set_tRNS(png_ptr, info_ptr, trans, paletteSize, NULL);
    }
  }


  png_write_info(png_ptr,info_ptr);

  int png_nbplanes = (do_palette? 1 : nbplanes);
  png_bytep row = (png_bytep) png_malloc(png_ptr,png_nbplanes * bytes_per_value * image.cols());

  for (size_t r = 0; r < (size_t) image.rows(); r++) {
    if (bytes_per_value == 1 && !do_palette) {
     for (size_t c = 0; c < (size_t) image.cols(); c++) {
      for (int p=0; p<nbplanes; p++) {
        row[c * nbplanes + p] = (png_byte) (image(p,r,c));
      }
     }
    } else if (do_palette) {
     for (size_t c = 0; c < (size_t) image.cols(); c++) {
        row[c] = (png_byte) (image(1,r,c));
     }
    } else {
     for (size_t c = 0; c < (size_t) image.cols(); c++) {
      for (int p=0; p<nbplanes; p++) {
        row[c * nbplanes * 2 + 2*p] = (png_byte) (image(p,r,c) >> 8);
        row[c * nbplanes * 2 + 2*p + 1] = (png_byte) (image(p,r,c) & 0xff);
      }
     }
    }
    png_write_row(png_ptr,row);
  }

  png_free(png_ptr,row);
  if (palette) png_free(png_ptr,palette);
  png_write_end(png_ptr,info_ptr);
  png_destroy_write_struct(&png_ptr,&info_ptr);
  fclose(fp);
  return 0;
#endif
}
