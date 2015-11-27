#include <string>
#include <string.h>

#include "maniac/rac.hpp"
#include "maniac/compound.hpp"
#include "maniac/util.hpp"

#include "image/color_range.hpp"
#include "transform/factory.hpp"

#include "flif_config.h"

#include "common.hpp"
#include "fileio.hpp"

using namespace maniac::util;

template<typename RAC> std::string static read_name(RAC& rac, uint8_t &transform_l)
{
    UniformSymbolCoder<RAC> coder(rac);
    int nb = transform_l + coder.read_int(0, MAX_TRANSFORM-transform_l);
    if (nb>MAX_TRANSFORM) nb=MAX_TRANSFORM;
    transform_l = nb+1;
    return transforms[nb];
}

template<typename IO, typename Rac, typename Coder> void flif_decode_scanlines_inner(IO &io, Rac &rac, std::vector<Coder> &coders, Images &images, const ColorRanges *ranges, std::vector<Transform<IO>*> &transforms, uint32_t (*callback)(int,int), Images &partial_images) {
    ColorVal min,max;
    const int nump = images[0].numPlanes();
    const bool alphazero = images[0].alpha_zero_special;
    const bool FRA = (nump == 5);
    int progressive_qual_target = 0;
    if (callback) {
         // initialize planes to grey
         for (int p=0; p<nump; p++) {
           if (ranges->min(p) < ranges->max(p))
           for (int fr=0; fr< (int)images.size(); fr++) {
            for (uint32_t r=0; r<images[fr].rows(); r++) {
              for (uint32_t c=0; c<images[fr].cols(); c++) {
                images[fr].set(p,r,c,(ranges->min(p)+ranges->max(p))/2);
              }
            }
           }
         }
    }
    for (int k=0,i=0; k < 5; k++) {
        int p=PLANE_ORDERING[k];
        if (p>=nump) continue;
        i++;
        Properties properties((nump>3?NB_PROPERTIES_scanlinesA[p]:NB_PROPERTIES_scanlines[p]));
        if (ranges->min(p) < ranges->max(p)) {
          v_printf(2,"\r%i%% done [%i/%i] DEC[%ux%u]    ",(int)(100*pixels_done/pixels_todo),i,nump,images[0].cols(),images[0].rows());
          v_printf(4,"\n");
          pixels_done += images[0].cols()*images[0].rows();
          for (uint32_t r = 0; r < images[0].rows(); r++) {
            for (int fr=0; fr< (int)images.size(); fr++) {
              Image& image = images[fr];
              uint32_t begin=image.col_begin[r], end=image.col_end[r];
              if (image.seen_before >= 0) { for(uint32_t c=0; c<image.cols(); c++) image.set(p,r,c,images[image.seen_before](p,r,c)); continue; }
              if (fr>0) {
                for (uint32_t c = 0; c < begin; c++)
                   if (alphazero && p<3 && image(3,r,c) == 0) image.set(p,r,c,predict(image,p,r,c));
                   else if (p !=4 ) image.set(p,r,c,images[fr-1](p,r,c));
              } else if (nump>3 && p<3) { begin=0; end=image.cols(); }
              for (uint32_t c = begin; c < end; c++) {
                if (alphazero && p<3 && image(3,r,c) == 0) {image.set(p,r,c,predict(image,p,r,c)); continue;}
                if (FRA && p<4 && image(4,r,c) > 0) {assert(fr >= image(4,r,c)); image.set(p,r,c,images[fr-image(4,r,c)](p,r,c)); continue;}
                ColorVal guess = predict_and_calcProps_scanlines(properties,ranges,image,p,r,c,min,max);
                if (FRA && p==4 && max > fr) max = fr;
                ColorVal curr = coders[p].read_int(properties, min - guess, max - guess) + guess;
                image.set(p,r,c, curr);
              }
              if (fr>0) {
                for (uint32_t c = end; c < image.cols(); c++)
                   if (alphazero && p<3 && image(3,r,c) == 0) image.set(p,r,c,predict(image,p,r,c));
                   else if (p !=4 ) image.set(p,r,c,images[fr-1](p,r,c));
              }
            }
          }
          int qual = 10000*pixels_done/pixels_todo;
          if (callback && p != 4 && qual >= progressive_qual_target) {
            for (unsigned int n=0; n < images.size(); n++) partial_images[n] = images[n].clone(); // make a copy to work with
            for (int i=transforms.size()-1; i>=0; i--) if (transforms[i]->undo_redo_during_decode()) transforms[i]->invData(partial_images);
            progressive_qual_target = callback(qual,io.ftell());
            if (qual >= progressive_qual_target) break;
          }
        }
    }
}

template<typename IO, typename Rac, typename Coder> void flif_decode_scanlines_pass(IO& io, Rac &rac, Images &images, const ColorRanges *ranges, std::vector<Tree> &forest, std::vector<Transform<IO>*> &transforms, uint32_t (*callback)(int,int), Images &partial_images, int cutoff = 2, int alpha = 0xFFFFFFFF / 19) {
    std::vector<Coder> coders;
    coders.reserve(images[0].numPlanes());
    for (int p = 0; p < images[0].numPlanes(); p++) {
        Ranges propRanges;
        initPropRanges_scanlines(propRanges, *ranges, p);
        coders.emplace_back(rac, propRanges, forest[p], 0, cutoff, alpha);
    }
    flif_decode_scanlines_inner<IO,Rac,Coder>(io, rac, coders, images, ranges, transforms, callback, partial_images);
}

// interpolate rest of the image
// used when decoding lossy
void flif_decode_FLIF2_inner_interpol(Images &images, const ColorRanges *ranges, const int I,
                                      const int beginZL, const int endZL, const int32_t R, const int scale) {
    for (int i = I; i < plane_zoomlevels(images[0], beginZL, endZL); i++) {
      if (i<0) continue;
      std::pair<int, int> pzl = plane_zoomlevel(images[0], beginZL, endZL, i);
      int p = pzl.first;
      int z = pzl.second;
      if (ranges->min(p) >= ranges->max(p)) continue;
      if ( p == 4 ) continue; // don't interpolate FRA lookback channel
      if ( 1<<(z/2) < scale) continue;
      pixels_done += images[0].cols(z)*images[0].rows(z)/2;
      v_printf(2,"\r%i%% done [%i/%i] INTERPOLATE[%i,%ux%u]                 ",(int)(100*pixels_done/pixels_todo),i,plane_zoomlevels(images[0], beginZL, endZL)-1,p,images[0].cols(z),images[0].rows(z));
      v_printf(5,"\n");

      if (z % 2 == 0) {
        // horizontal: scan the odd rows
          for (uint32_t r = (I==i && R>=0 ? R : 1); r < images[0].rows(z); r += 2) {
            for (Image& image : images) {
              if (image.palette == false) {
               for (uint32_t c = 0; c < image.cols(z); c++) {
                 image.set(p,z,r,c, predict(image,z,p,r,c));    // normal method: use predict() for interpolation
               }
              } else {
               for (uint32_t c = 0; c < image.cols(z); c++) {
                 image.set(p,z,r,c, image(p,z,r-1,c));          // paletted image: no interpolation
               }
              }
            }
          }
      } else {
        // vertical: scan the odd columns
          for (uint32_t r = (I==i && R>=0 ? R : 0); r < images[0].rows(z); r++) {
            for (Image& image : images) {
              if (image.palette == false) {
               for (uint32_t c = 1; c < image.cols(z); c += 2) {
                image.set(p,z,r,c, predict(image,z,p,r,c));
               }
              } else {
               for (uint32_t c = 1; c < image.cols(z); c += 2) {
                image.set(p,z,r,c, image(p,z,r,c-1));
               }
              }
            }
          }
      }
    }
    v_printf(2,"\n");
}


template<typename IO, typename Rac, typename Coder> void flif_decode_FLIF2_inner(IO& io, Rac &rac, std::vector<Coder> &coders, Images &images, const ColorRanges *ranges, const int beginZL, const int endZL, int quality, int scale, std::vector<Transform<IO>*> &transforms, uint32_t (*callback)(int,int), Images &partial_images) {
    ColorVal min,max;
    const int nump = images[0].numPlanes();
    const bool alphazero = images[0].alpha_zero_special;
    const bool FRA = (nump == 5);
    // flif_decode
    for (int i = 0; i < plane_zoomlevels(images[0], beginZL, endZL); i++) {
      std::pair<int, int> pzl = plane_zoomlevel(images[0], beginZL, endZL, i);
      int p = pzl.first;
      int z = pzl.second;
      if ((100*pixels_done > quality*pixels_todo) ||  1<<(z/2) < scale) {
              flif_decode_FLIF2_inner_interpol(images, ranges, i, beginZL, endZL, (z%2 == 0 ?1:0), scale);
              return;
      }
      if (ranges->min(p) < ranges->max(p)) {
      if (endZL == 0) v_printf(2,"\r%i%% done [%i/%i] DEC[%i,%ux%u]  ",(int)(100*pixels_done/pixels_todo),i,plane_zoomlevels(images[0], beginZL, endZL)-1,p,images[0].cols(z),images[0].rows(z));
      pixels_done += (images[0].cols(z)/(z%2==0?1:2))*(images[0].rows(z)/(z%2==0?2:1));
      ColorVal curr;
      Properties properties((nump>3?NB_PROPERTIESA[p]:NB_PROPERTIES[p]));
      if (z % 2 == 0) {
          for (uint32_t r = 1; r < images[0].rows(z); r += 2) {
#ifdef CHECK_FOR_BROKENFILES
            if (io.isEOF()) {
              v_printf(1,"Row %i: Unexpected file end. Interpolation from now on.\n",r);
              flif_decode_FLIF2_inner_interpol(images, ranges, i, beginZL, endZL, (r>1?r-2:r), scale);
              return;
            }
#endif
            for (int fr=0; fr<(int)images.size(); fr++) {
              Image& image = images[fr];
              if (image.seen_before >= 0) { for (uint32_t c=0; c<image.cols(z); c++) image.set(p,z,r,c,images[image.seen_before](p,z,r,c)); continue; }
              uint32_t begin=image.col_begin[r*image.zoom_rowpixelsize(z)]/image.zoom_colpixelsize(z), end=1+(image.col_end[r*image.zoom_rowpixelsize(z)]-1)/image.zoom_colpixelsize(z);
              if (fr>0) {
                for (uint32_t c = 0; c < begin; c++)
                            if (alphazero && p<3 && image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c));
                            else if (p !=4 ) image.set(p,z,r,c,images[fr-1](p,z,r,c));
                for (uint32_t c = end; c < image.cols(z); c++)
                            if (alphazero && p<3 && image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c));
                            else if (p !=4 ) image.set(p,z,r,c,images[fr-1](p,z,r,c));
              } else {
                if (nump>3 && p<3) {begin=0; end=image.cols(z);}
              }
              for (uint32_t c = begin; c < end; c++) {
                     if (alphazero && p<3 && image(3,z,r,c) == 0) { image.set(p,z,r,c,predict(image,z,p,r,c)); continue;}
                     if (FRA && p<4 && image(4,z,r,c) > 0) { image.set(p,z,r,c,images[fr-image(4,z,r,c)](p,z,r,c)); continue;}
                     ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max);
                     if (FRA && p==4 && max > fr) max = fr;
                     curr = coders[p].read_int(properties, min - guess, max - guess) + guess;
                     image.set(p,z,r,c, curr);
              }
            }
        }
      } else {
          for (uint32_t r = 0; r < images[0].rows(z); r++) {
#ifdef CHECK_FOR_BROKENFILES
            if (io.isEOF()) {
              v_printf(1,"Row %i: Unexpected file end. Interpolation from now on.\n", r);
              flif_decode_FLIF2_inner_interpol(images, ranges, i, beginZL, endZL, (r>0?r-1:r), scale);
              return;
            }
#endif
            for (int fr=0; fr<(int)images.size(); fr++) {
              Image& image = images[fr];
              if (image.seen_before >= 0) { for (uint32_t c=1; c<image.cols(z); c+=2) image.set(p,z,r,c,images[image.seen_before](p,z,r,c)); continue; }
              uint32_t begin=(image.col_begin[r*image.zoom_rowpixelsize(z)]/image.zoom_colpixelsize(z)),
              end=(1+(image.col_end[r*image.zoom_rowpixelsize(z)]-1)/image.zoom_colpixelsize(z))|1;
              if (begin>1 && ((begin&1) ==0)) begin--;
              if (begin==0) begin=1;
              if (fr>0) {
                for (uint32_t c = 1; c < begin; c+=2)
                            if (alphazero && p<3 && image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c));
                            else if (p !=4 ) image.set(p,z,r,c,images[fr-1](p,z,r,c));
                for (uint32_t c = end; c < image.cols(z); c+=2)
                            if (alphazero && p<3 && image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c));
                            else if (p !=4 ) image.set(p,z,r,c,images[fr-1](p,z,r,c));
              } else {
                if (nump>3 && p<3) {begin=1; end=image.cols(z);}
              }
              for (uint32_t c = begin; c < end; c+=2) {
                     if (alphazero && p<3 && image(3,z,r,c) == 0) { image.set(p,z,r,c,predict(image,z,p,r,c)); continue;}
                     if (FRA && p<4 && image(4,z,r,c) > 0) { image.set(p,z,r,c,images[fr-image(4,z,r,c)](p,z,r,c)); continue;}
                     ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max);
                     if (FRA && p==4 && max > fr) max = fr;
                     curr = coders[p].read_int(properties, min - guess, max - guess) + guess;
                     image.set(p,z,r,c, curr);
              }
            }
        }
      }
      if (endZL==0) {
          v_printf(3,"    read %li bytes   ", io.ftell());
          v_printf(5,"\n");
      }
      int qual = 10000*pixels_done/pixels_todo;
      if (callback && p<4 && (endZL==0 || i+1 == plane_zoomlevels(images[0], beginZL, endZL)) && qual >= progressive_qual_target) {
        for (unsigned int n=0; n < images.size(); n++) partial_images[n] = images[n].clone(); // make a copy to work with
        int64_t pixels_really_done = pixels_done;
        flif_decode_FLIF2_inner_interpol(partial_images, ranges, i+1, beginZL, endZL, -1, scale);
        if (endZL>0) flif_decode_FLIF2_inner_interpol(partial_images, ranges, 0, endZL-1, 0, -1, scale);
        pixels_done = pixels_really_done;
        for (int i=transforms.size()-1; i>=0; i--) if (transforms[i]->undo_redo_during_decode()) transforms[i]->invData(partial_images);
        progressive_qual_target = callback(qual,io.ftell());
        if (qual >= progressive_qual_target) break;
      }
      }
    }
}

template<typename IO, typename Rac, typename Coder> void flif_decode_FLIF2_pass(IO &io, Rac &rac, Images &images, const ColorRanges *ranges, std::vector<Tree> &forest, const int beginZL, const int endZL, int quality, int scale, std::vector<Transform<IO>*> &transforms, uint32_t (*callback)(int,int), Images &partial_images, int cutoff = 2, int alpha = 0xFFFFFFFF / 19) {
    std::vector<Coder> coders;
    coders.reserve(images[0].numPlanes());
    for (int p = 0; p < images[0].numPlanes(); p++) {
        Ranges propRanges;
        initPropRanges(propRanges, *ranges, p);
        coders.emplace_back(rac, propRanges, forest[p], 0, cutoff, alpha);
    }

    for (Image& image : images)
    if (beginZL == image.zooms()) {
      // special case: very left top pixel must be read first to get it all started
      SimpleSymbolCoder<FLIFBitChanceMeta, Rac, 24> metaCoder(rac);
      for (int p = 0; p < image.numPlanes(); p++) {
        if (ranges->min(p) < ranges->max(p)) {
          image.set(p,0,0, metaCoder.read_int(ranges->min(p), ranges->max(p)));
          pixels_done++;
        }
      }
    }
    flif_decode_FLIF2_inner<IO,Rac,Coder>(io, rac, coders, images, ranges, beginZL, endZL, quality, scale, transforms, callback, partial_images);
}



template<typename IO, typename BitChance, typename Rac> bool flif_decode_tree(IO& io, Rac &rac, const ColorRanges *ranges, std::vector<Tree> &forest, const int encoding)
{
    try {
      for (int p = 0; p < ranges->numPlanes(); p++) {
        Ranges propRanges;
        if (encoding==1) initPropRanges_scanlines(propRanges, *ranges, p);
        else initPropRanges(propRanges, *ranges, p);
        MetaPropertySymbolCoder<BitChance, Rac> metacoder(rac, propRanges);
        if (ranges->min(p)<ranges->max(p))
        if (!metacoder.read_tree(forest[p])) {return false;}
//        forest[p].print(stdout);
      }
    } catch (std::bad_alloc& ba) {
        e_printf("Error: could not allocate enough memory for MANIAC trees.\n");
        return false;
      }
    return true;
}

template <int bits, typename IO>
bool flif_decode_main(RacIn<IO>& rac, IO& io, Images &images, const ColorRanges *ranges,
        std::vector<Transform<IO>*> &transforms, int quality, int scale, uint32_t (*callback)(int,int), Images &partial_images, int encoding, int cutoff = 2, int alpha = 0xFFFFFFFF / 19) {

    std::vector<Tree> forest(ranges->numPlanes(), Tree());
    int roughZL = 0;
    if (encoding == 2) {
      roughZL = images[0].zooms() - NB_NOLEARN_ZOOMS-1;
      if (roughZL < 0) roughZL = 0;
//      v_printf(2,"Decoding rough data\n");
      flif_decode_FLIF2_pass<IO, RacIn<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn<IO>, bits> >(io, rac, images, ranges, forest, images[0].zooms(), roughZL+1, 100, scale, transforms, callback, partial_images, cutoff, alpha);
    }
    if (encoding == 2 && quality <= 0) {
      v_printf(3,"Not decoding MANIAC tree\n");
    } else {
      v_printf(3,"Decoded header + rough data. Decoding MANIAC tree.\n");
      if (!flif_decode_tree<IO, FLIFBitChanceTree, RacIn<IO>>(io, rac, ranges, forest, encoding)) return false;
    }


    switch(encoding) {
        case 1: v_printf(3,"Decoding data (scanlines)\n");
                flif_decode_scanlines_pass<IO, RacIn<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn<IO>, bits> >(io, rac, images, ranges, forest, transforms, callback, partial_images, cutoff, alpha);
                break;
        case 2: v_printf(3,"Decoding data (interlaced)\n");
                flif_decode_FLIF2_pass<IO, RacIn<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn<IO>, bits> >(io, rac, images, ranges, forest, roughZL, 0, quality, scale, transforms, callback, partial_images, cutoff, alpha);
                break;
    }
    return true;
}

template <typename IO>
bool flif_decode(IO& io, Images &images, int quality, int scale, uint32_t (*callback)(int,int), Images &partial_images) {
    bool just_identify = false;
    if (scale == -1) just_identify=true;
    else if (scale != 1 && scale != 2 && scale != 4 && scale != 8 && scale != 16 && scale != 32 && scale != 64 && scale != 128) {
                e_printf("Invalid scale down factor: %i\n", scale);
                return false;
    }

    char buff[5];
    if (!io.gets(buff,5)) { e_printf("Could not read header from file: %s\n",io.getName()); return false; }
    if (!strcmp(buff,"!<ar")) {
       // FLIF file in an archive, try to find find the main image
       if (!io.gets(buff,5)) return false;
       if (strcmp(buff,"ch>\n")) return false;
       char ar_header[61];
       while (true) {
          if (!io.gets(ar_header,61)) { e_printf("Archive does not contain a FLIF image\n"); return false; }
          if (!strncmp(ar_header,"__image.flif/",13)) {
            if (!io.gets(buff,5)) { e_printf("Corrupt archive?\n"); return false; }
            break;
          }
          else {
            long skip = strtol(&ar_header[48],NULL,10);
            if (skip < 0) return false;
            if (skip & 1) skip++;
            io.fseek(skip,SEEK_CUR);
          }
       }
    }
    if (strcmp(buff,"FLIF")) { e_printf("%s is not a FLIF file\n",io.getName()); return false; }
    int c;
    if (!ioget_int_8bit (io, &c))
        return false;
    if (c < ' ' || c > ' '+32+15+32) { e_printf("Invalid or unknown FLIF format byte\n"); return false;}
    c -= ' ';
    int numFrames=1;
    if (c > 47) {
        c -= 32;
        if (!ioget_int_8bit (io, &numFrames))
            return false;
        if (numFrames < 2 || numFrames >= 256) return false;
        if (numFrames == 0xff) {
          if (!ioget_int_16bit_bigendian (io, &numFrames))
            return false;
        }
    }
    const int encoding=c/16;
    if (encoding < 1 || encoding > 2) { e_printf("Invalid or unknown FLIF encoding method\n"); return false;}
    if (scale != 1 && encoding==1 && !just_identify) { v_printf(1,"Cannot decode non-interlaced FLIF file at lower scale! Ignoring scale...\n");}
    if (quality < 100 && encoding==1) { v_printf(1,"Cannot decode non-interlaced FLIF file at lower quality! Ignoring quality...\n");}
    int numPlanes=c%16;
    if (numPlanes < 1 || numPlanes > 4) {e_printf("Invalid FLIF header (unsupported color channels)\n"); return false;}
    if (!ioget_int_8bit (io, &c))
        return false;
    if (c < '0' || c > '2')  {e_printf("Invalid FLIF header (unsupported color depth)\n"); return false;}

    int width;
    int height;
    if (!ioget_int_16bit_bigendian (io, &width))
        return false;
    if (!ioget_int_16bit_bigendian (io, &height))
        return false;
    if (width < 1 || height < 1) {e_printf("Invalid FLIF header\n"); return false;}

    // TODO: implement downscaled decoding without allocating a fullscale image buffer!

    RacIn<IO> rac(io);
    SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 24> metaCoder(rac);

//    image.init(width, height, 0, 0, 0);
    v_printf(3,"Decoding %ux%u image, channels:",width,height);
    int maxmax=0;
    for (int p = 0; p < numPlanes; p++) {
//        int min = 0;
        int max = 255;
        if (c=='2') max=65535;
        else if (c=='0') max=(1 << metaCoder.read_int(1, 16)) - 1;
        if (max>maxmax) maxmax=max;
//        image.add_plane(min, max);
//        v_printf(2," [%i] %i bpp (%i..%i)",p,ilog2(image.max(p)+1),image.min(p), image.max(p));
        if (c=='0') v_printf(3," [%i] %i bpp",p,ilog2(max+1));
    }
    if (c=='1') v_printf(3," %i, depth: 8 bit",numPlanes);
    if (c=='2') v_printf(3," %i, depth: 16 bit",numPlanes);
    if (numFrames>1) v_printf(3,", frames: %i",numFrames);
    bool alphazero=false;
    if (numPlanes>3) {
        alphazero=metaCoder.read_int(0, 1);
        if (!alphazero) v_printf(3, ", store RGB at A=0");
    }
    v_printf(3,"\n");
    if (just_identify) {
        v_printf(1,"%s: ",io.getName());
        if (numFrames == 1) v_printf(1,"FLIF image");
        else v_printf(1,"FLIF animation, %i frames",numFrames);
        v_printf(1,", %ux%u, ", width, height);
        if (c=='1') v_printf(1,"8-bit ");
        else if (c=='2') v_printf(1,"16-bit ");
        else if (c=='0') v_printf(1,"%i-bit ", ilog2(maxmax+1));
        if (numPlanes == 1) v_printf(1,"grayscale");
        else if (numPlanes == 3) v_printf(1,"RGB");
        else if (numPlanes == 4) v_printf(1,"RGBA");
        if (encoding == 1) v_printf(1,", non-interlaced");
        else if (encoding == 2) v_printf(1,", interlaced");
        v_printf(1,"\n");
        return true;
    }
    if (numFrames>1) {
        // ignored for now (assuming loop forever)
        metaCoder.read_int(0, 100); // repeats (0=infinite)
    }

    for (int i=0; i<numFrames; i++) {
      images.push_back(Image());
      if (!images[i].init(width,height,0,maxmax,numPlanes)) return false;
      images[i].alpha_zero_special = alphazero;
      if (numFrames>1) images[i].frame_delay = metaCoder.read_int(0, 60000); // time in ms between frames
      if (callback) partial_images.push_back(Image());
      //if (numFrames>1) partial_images[i].frame_delay = images[i].frame_delay;
    }

    int cutoff = metaCoder.read_int(1,128);
    int alpha = 0xFFFFFFFF / metaCoder.read_int(4,128);
    std::vector<const ColorRanges*> rangesList;
    std::vector<Transform<IO>*> transforms;
    rangesList.push_back(getRanges(images[0]));
    v_printf(4,"Transforms: ");
    int tcount=0;
    uint8_t transform_l=0;

    while (rac.read_bit()) {
        if (transform_l > MAX_TRANSFORM) return false;
        std::string desc = read_name(rac, transform_l);
        Transform<IO> *trans = create_transform<IO>(desc);
        if (!trans) {
            e_printf("Unknown transformation '%s'\n", desc.c_str());
            return false;
        }
        if (!trans->init(rangesList.back())) {
            e_printf("Transformation '%s' failed\n", desc.c_str());
            return false;
        }
        if (tcount++ > 0) v_printf(4,", ");
        v_printf(4,"%s", desc.c_str());
        if (desc == "FRA") {
                if (images.size()<2) return false;
        }
        if (desc == "FRS") {
                if (images.size()<2) return false;
                int unique_frames=images.size()-1; // not considering first frame
                for (Image& i : images) if (i.seen_before >= 0) unique_frames--;
                if (unique_frames < 1) {return false;}
                trans->configure(unique_frames*images[0].rows()); trans->configure(images[0].cols()); }
        if (desc == "DUP") { if (images.size()<2) return false; else trans->configure(images.size()); }
        if (desc == "PLA") { trans->configure(images[0].alpha_zero_special); }
        if (!trans->load(rangesList.back(), rac)) return false;
        rangesList.push_back(trans->meta(images, rangesList.back()));
        transforms.push_back(trans);
    }
    if (tcount==0) v_printf(4,"none\n"); else v_printf(4,"\n");
    const ColorRanges* ranges = rangesList.back();
    grey.clear();
    for (int p = 0; p < ranges->numPlanes(); p++) grey.push_back((ranges->min(p)+ranges->max(p))/2);

    pixels_todo = (int64_t)width*height*ranges->numPlanes()/scale/scale;
    pixels_done = 0;
    progressive_qual_target = 0;

    for (int p = 0; p < ranges->numPlanes(); p++) {
      v_printf(7,"Plane %i: %i..%i\n",p,ranges->min(p),ranges->max(p));
    }

    for (int p = 0; p < ranges->numPlanes(); p++) {
        if (ranges->min(p) >= ranges->max(p)) {
            v_printf(5,"Constant plane %i at color value %i\n",p,ranges->min(p));
            pixels_todo -= (int64_t)width*height/scale/scale;
            for (int fr = 0; fr < numFrames; fr++)
                images[fr].make_constant_plane(p,ranges->min(p));
        }
    }
    int mbits = 0;
    for (int p = 0; p < ranges->numPlanes(); p++) {
        if (ranges->max(p) > ranges->min(p)) {
          int nBits = ilog2((ranges->max(p) - ranges->min(p))*2-1)+1;
          if (nBits > mbits) mbits = nBits;
        }
    }
    int bits = 10;
#ifdef SUPPORT_HDR
    if (mbits >10) bits=18;
    if (mbits > bits) { e_printf("FLIF cannot decode >16 bit per channel. How did this happen?\n"); return false;}
#else
    if (mbits > bits) { e_printf("This FLIF cannot decode >8 bit per channel. Please compile with SUPPORT_HDR.\n"); return false;}
#endif

    if (bits == 10) {
       if (!flif_decode_main<10>(rac, io, images, ranges, transforms, quality, scale, callback, partial_images, encoding, cutoff, alpha)) return false;
#ifdef SUPPORT_HDR
    } else {
       if (!flif_decode_main<18>(rac, io, images, ranges, transforms, quality, scale, callback, partial_images, encoding, cutoff, alpha)) return false;
#endif
    }


    if (quality==100 && scale==1) {
      const uint32_t checksum = images[0].checksum();
      v_printf(8,"Computed checksum: %X\n", checksum);
      uint32_t checksum2 = metaCoder.read_int(0, 0xFFFF);
      checksum2 *= 0x10000;
      checksum2 += metaCoder.read_int(0, 0xFFFF);
      v_printf(8,"Read checksum: %X\n", checksum2);
      if (checksum != checksum2) {
        v_printf(1,"\nCORRUPTION DETECTED: checksums don't match (computed: %x v/s read: %x)! (partial file?)\n\n", checksum, checksum2);
      } else {
        v_printf(2,"Image decoded, checksum verified.\n");
      }
    } else {
      v_printf(2,"Not checking checksum, lossy partial decoding was chosen.\n");
    }

   if (numFrames==1)
      v_printf(2,"\rDecoding done, %li bytes for %ux%u pixels (%.4fbpp)   \n",io.ftell(), images[0].cols()/scale, images[0].rows()/scale, 8.0*io.ftell()/images[0].rows()/images[0].cols()/scale/scale);
    else
      v_printf(2,"\rDecoding done, %li bytes for %i frames of %ux%u pixels (%.4fbpp)   \n",io.ftell(), numFrames, images[0].cols()/scale, images[0].rows()/scale, 8.0*io.ftell()/numFrames/images[0].rows()/images[0].cols()/scale/scale);


    for (int i=(int)transforms.size()-1; i>=0; i--) {
        transforms[i]->invData(images);
        delete transforms[i];
    }
    transforms.clear();


    for (unsigned int i=0; i<rangesList.size(); i++) {
        delete rangesList[i];
    }
    rangesList.clear();

    return true;
}


template bool flif_decode(FileIO& io, Images &images, int quality, int scale, uint32_t (*callback)(int,int), Images &partial_images);
template bool flif_decode(BlobReader& io, Images &images, int quality, int scale, uint32_t (*callback)(int,int), Images &partial_images);
