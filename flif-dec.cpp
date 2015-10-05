#include <string>
#include <string.h>

#include "maniac/rac.h"
#include "maniac/compound.h"
#include "maniac/util.h"

#include "image/color_range.h"
#include "transform/factory.h"

#include "flif_config.h"

#include "common.h"
#include "fileio.h"

template<typename RAC> std::string static read_name(RAC& rac)
{
    UniformSymbolCoder<RAC> coder(rac);
    int nb = coder.read_int(0, MAX_TRANSFORM);
    return transforms[nb];
}

template<typename Coder> void flif_decode_scanlines_inner(std::vector<Coder*> &coders, Images &images, const ColorRanges *ranges)
{

    ColorVal min,max;
    int nump = images[0].numPlanes();
    int beginp = (nump>3 ? 3 : 0);
    for (int p = beginp, i=0; i++ < nump; p = (p+1)%nump) {
        Properties properties((nump>3?NB_PROPERTIES_scanlinesA[p]:NB_PROPERTIES_scanlines[p]));
        v_printf(2,"\r%i%% done [%i/%i] DEC[%ux%u]    ",(int)(100*pixels_done/pixels_todo),i,nump,images[0].cols(),images[0].rows());
        v_printf(4,"\n");
        pixels_done += images[0].cols()*images[0].rows();
        if (ranges->min(p) >= ranges->max(p)) continue;
        for (uint32_t r = 0; r < images[0].rows(); r++) {
            for (int fr=0; fr< (int)images.size(); fr++) {
              Image& image = images[fr];
              uint32_t begin=image.col_begin[r], end=image.col_end[r];
              if (image.seen_before >= 0) { for(uint32_t c=0; c<image.cols(); c++) image.set(p,r,c,images[image.seen_before](p,r,c)); continue; }
              if (fr>0) {
                for (uint32_t c = 0; c < begin; c++)
                   if (nump>3 && p<3 && image(3,r,c) == 0) image.set(p,r,c,predict_and_calcProps_scanlines(properties,ranges,image,p,r,c,min,max));
                   else {
                     int oldframe=fr-1;  image.set(p,r,c,images[oldframe](p,r,c));
                     while(p == 3 && image(p,r,c) < 0) {oldframe += image(p,r,c); assert(oldframe>=0); image.set(p,r,c,images[oldframe](p,r,c));}
                   }
              } else {
                if (nump>3 && p<3) { begin=0; end=image.cols(); }
              }
              for (uint32_t c = begin; c < end; c++) {
                ColorVal guess = predict_and_calcProps_scanlines(properties,ranges,image,p,r,c,min,max);
                if (p==3 && min < -fr) min = -fr;
                if (nump>3 && p<3 && image(3,r,c) <= 0) { if (image(3,r,c) == 0) image.set(p,r,c,guess); else image.set(p,r,c,images[fr+image(3,r,c)](p,r,c)); continue;}
                ColorVal curr = coders[p]->read_int(properties, min - guess, max - guess) + guess;
                image.set(p,r,c, curr);
              }
              if (fr>0) {
                for (uint32_t c = end; c < image.cols(); c++)
                   if (nump>3 && p<3 && image(3,r,c) == 0) image.set(p,r,c,predict_and_calcProps_scanlines(properties,ranges,image,p,r,c,min,max));
                   else {
                     int oldframe=fr-1;  image.set(p,r,c,images[oldframe](p,r,c));
                     while(p == 3 && image(p,r,c) < 0) {oldframe += image(p,r,c); assert(oldframe>=0); image.set(p,r,c,images[oldframe](p,r,c));}
                   }
              }
            }
        }
    }
}

template<typename Rac, typename Coder> void flif_decode_scanlines_pass(Rac &rac, Images &images, const ColorRanges *ranges, std::vector<Tree> &forest)
{
    std::vector<Coder*> coders;
    for (int p = 0; p < images[0].numPlanes(); p++) {
        Ranges propRanges;
        initPropRanges_scanlines(propRanges, *ranges, p);
        coders.push_back(new Coder(rac, propRanges, forest[p]));
    }
    flif_decode_scanlines_inner(coders, images, ranges);
    for (int p = 0; p < images[0].numPlanes(); p++) {
        delete coders[p];
    }
}

// interpolate rest of the image
// used when decoding lossy
void flif_decode_FLIF2_inner_interpol(Images &images, const ColorRanges *ranges, const int I, const int beginZL, const int endZL, const uint32_t R, const int scale)
{
    for (int i = I; i < plane_zoomlevels(images[0], beginZL, endZL); i++) {
      std::pair<int, int> pzl = plane_zoomlevel(images[0], beginZL, endZL, i);
      int p = pzl.first;
      int z = pzl.second;
      if ( 1<<(z/2) < scale) continue;
      pixels_done += images[0].cols(z)*images[0].rows(z)/2;
      v_printf(2,"\r%i%% done [%i/%i] INTERPOLATE[%i,%ux%u]                 ",(int)(100*pixels_done/pixels_todo),i,plane_zoomlevels(images[0], beginZL, endZL)-1,p,images[0].cols(z),images[0].rows(z));
      v_printf(5,"\n");

      if (z % 2 == 0) {
        // horizontal: scan the odd rows
          for (uint32_t r = (I==i?R:1); r < images[0].rows(z); r += 2) {
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
          for (uint32_t r = (I==i?R:0); r < images[0].rows(z); r++) {
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

template<typename Rac, typename Coder> void flif_decode_FLIF2_inner(Rac &rac, std::vector<Coder*> &coders, Images &images, const ColorRanges *ranges, const int beginZL, const int endZL, int quality, int scale)
{
    ColorVal min,max;
    int nump = images[0].numPlanes();
//    if (quality >= 0) {
//      quality = plane_zoomlevels(image, beginZL, endZL) * quality / 100;
//    }
    // flif_decode
    for (int i = 0; i < plane_zoomlevels(images[0], beginZL, endZL); i++) {
      std::pair<int, int> pzl = plane_zoomlevel(images[0], beginZL, endZL, i);
      int p = pzl.first;
      int z = pzl.second;
      if ((100*pixels_done > quality*pixels_todo) ||  1<<(z/2) < scale) {
              flif_decode_FLIF2_inner_interpol(images, ranges, i, beginZL, endZL, (z%2 == 0 ?1:0), scale);
              return;
      }
      if (endZL == 0) v_printf(2,"\r%i%% done [%i/%i] DEC[%i,%ux%u]  ",(int)(100*pixels_done/pixels_todo),i,plane_zoomlevels(images[0], beginZL, endZL)-1,p,images[0].cols(z),images[0].rows(z));
      pixels_done += images[0].cols(z)*images[0].rows(z)/2;
      if (ranges->min(p) >= ranges->max(p)) continue;
      ColorVal curr;
      Properties properties((nump>3?NB_PROPERTIESA[p]:NB_PROPERTIES[p]));
      if (z % 2 == 0) {
          for (uint32_t r = 1; r < images[0].rows(z); r += 2) {
#ifdef CHECK_FOR_BROKENFILES
            if (rac.isEOF()) {
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
                            if (nump>3 && p<3 && image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c));
                            else { int oldframe=fr-1;  image.set(p,z,r,c,images[oldframe](p,z,r,c));
                                   while(p == 3 && image(p,z,r,c) < 0) {oldframe += image(p,z,r,c); assert(oldframe>=0); image.set(p,z,r,c,images[oldframe](p,z,r,c));}}
                for (uint32_t c = end; c < image.cols(z); c++)
                            if (nump>3 && p<3 && image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c));
                            else { int oldframe=fr-1;  image.set(p,z,r,c,images[oldframe](p,z,r,c));
                                   while(p == 3 && image(p,z,r,c) < 0) {oldframe += image(p,z,r,c); assert(oldframe>=0); image.set(p,z,r,c,images[oldframe](p,z,r,c));}}
              } else {
                if (nump>3 && p<3) { begin=0; end=image.cols(z); }
              }
              for (uint32_t c = begin; c < end; c++) {
                     if (nump>3 && p<3 && image(3,z,r,c) <= 0) { if (image(3,z,r,c) == 0) image.set(p,z,r,c,predict(image,z,p,r,c)); else image.set(p,z,r,c,images[fr+image(3,z,r,c)](p,z,r,c)); continue;}
                     ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max);
                     if (p==3 && min < -fr) min = -fr;
                     curr = coders[p]->read_int(properties, min - guess, max - guess) + guess;
                     image.set(p,z,r,c, curr);
              }
            }
        }
      } else {
          for (uint32_t r = 0; r < images[0].rows(z); r++) {
#ifdef CHECK_FOR_BROKENFILES
            if (rac.isEOF()) {
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
                     if (nump>3 && p<3 && image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c));
                     else { int oldframe=fr-1;  image.set(p,z,r,c,images[oldframe](p,z,r,c));
                            while(p == 3 && image(p,z,r,c) < 0) {oldframe += image(p,z,r,c); assert(oldframe>=0); image.set(p,z,r,c,images[oldframe](p,z,r,c));}}
                for (uint32_t c = end; c < image.cols(z); c+=2)
                     if (nump>3 && p<3 && image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c));
                     else { int oldframe=fr-1;  image.set(p,z,r,c,images[oldframe](p,z,r,c));
                            while(p == 3 && image(p,z,r,c) < 0) {oldframe += image(p,z,r,c); assert(oldframe>=0); image.set(p,z,r,c,images[oldframe](p,z,r,c));}}
              } else {
                if (nump>3 && p<3) { begin=1; end=image.cols(z); }
              }
              for (uint32_t c = begin; c < end; c+=2) {
                     if (nump>3 && p<3 && image(3,z,r,c) <= 0) { if (image(3,z,r,c) == 0) image.set(p,z,r,c,predict(image,z,p,r,c)); else image.set(p,z,r,c,images[fr+image(3,z,r,c)](p,z,r,c)); continue;}
                     ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max);
                     if (p==3 && min < -fr) min = -fr;
                     curr = coders[p]->read_int(properties, min - guess, max - guess) + guess;
                     image.set(p,z,r,c, curr);
              }
            }
        }
      }
      if (endZL==0) {
          v_printf(3,"    read %li bytes   ", rac.ftell());
          v_printf(5,"\n");
      }
    }
}

template<typename Rac, typename Coder> void flif_decode_FLIF2_pass(Rac &rac, Images &images, const ColorRanges *ranges, std::vector<Tree> &forest, const int beginZL, const int endZL, int quality, int scale)
{
    std::vector<Coder*> coders;
    for (int p = 0; p < images[0].numPlanes(); p++) {
        Ranges propRanges;
        initPropRanges(propRanges, *ranges, p);
        coders.push_back(new Coder(rac, propRanges, forest[p]));
    }

    for (Image& image : images)
    if (beginZL == image.zooms()) {
      // special case: very left top pixel must be read first to get it all started
      SimpleSymbolCoder<FLIFBitChanceMeta, Rac, 24> metaCoder(rac);
      for (int p = 0; p < image.numPlanes(); p++) {
        image.set(p,0,0, metaCoder.read_int(ranges->min(p), ranges->max(p)));
      }
    }

    flif_decode_FLIF2_inner(rac, coders, images, ranges, beginZL, endZL, quality, scale);

    for (int p = 0; p < images[0].numPlanes(); p++) {
        delete coders[p];
    }
}



template<typename BitChance, typename Rac> void flif_decode_tree(Rac &rac, const ColorRanges *ranges, std::vector<Tree> &forest, const int encoding)
{
    for (int p = 0; p < ranges->numPlanes(); p++) {
        Ranges propRanges;
        if (encoding==1) initPropRanges_scanlines(propRanges, *ranges, p);
        else initPropRanges(propRanges, *ranges, p);
        MetaPropertySymbolCoder<BitChance, Rac> metacoder(rac, propRanges);
        if (ranges->min(p)<ranges->max(p))
        metacoder.read_tree(forest[p]);
//        forest[p].print(stdout);
    }
}



template <typename IO>
bool flif_decode(IO io, Images &images, int quality, int scale)
{
    if (scale != 1 && scale != 2 && scale != 4 && scale != 8 && scale != 16 && scale != 32 && scale != 64 && scale != 128) {
                fprintf(stderr,"Invalid scale down factor: %i\n", scale);
                return false;
    }

    char buff[5];
    if (!io.gets(buff,5)) { fprintf(stderr,"Could not read header from file: %s\n",io.getName()); return false; }
    if (strcmp(buff,"FLIF")) { fprintf(stderr,"Not a FLIF file: %s\n",io.getName()); return false; }
    int c = io.getc()-' ';
    int numFrames=1;
    if (c > 47) {
        c -= 32;
        numFrames = io.getc();
    }
    int encoding=c/16;
    if (scale != 1 && encoding==1) { v_printf(1,"Cannot decode non-interlaced FLIF file at lower scale! Ignoring scale...\n");}
    if (quality < 100 && encoding==1) { v_printf(1,"Cannot decode non-interlaced FLIF file at lower quality! Ignoring quality...\n");}
    int numPlanes=c%16;
    c = io.getc();

    int width=io.getc() << 8;
    width += io.getc();
    int height=io.getc() << 8;
    height += io.getc();

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
    v_printf(3,"\n");

    if (numFrames>1) {
        for (int i=0; i<numFrames; i++) {
           metaCoder.read_int(0, 60000); // time in ms between frames
        }
    }

    for (int i=0; i<numFrames; i++) {
      Image image;
      images.push_back(image);
      images[i].init(width,height,0,maxmax,numPlanes);
    }
    std::vector<const ColorRanges*> rangesList;
    std::vector<Transform<IO>*> transforms;
    rangesList.push_back(getRanges(images[0]));
    v_printf(4,"Transforms: ");
    int tcount=0;
    while (rac.read()) {
        std::string desc = read_name(rac);
        Transform<IO> *trans = create_transform<IO>(desc);
        if (!trans) {
            fprintf(stderr,"Unknown transformation '%s'\n", desc.c_str());
            return false;
        }
        if (!trans->init(rangesList.back())) {
            fprintf(stderr,"Transformation '%s' failed\n", desc.c_str());
            return false;
        }
        if (tcount++ > 0) v_printf(4,", ");
        v_printf(4,"%s", desc.c_str());
        if (desc == "FRS") {
                int unique_frames=images.size()-1; // not considering first frame
                for (Image& i : images) if (i.seen_before >= 0) unique_frames--;
                trans->configure(unique_frames*images[0].rows()); trans->configure(images[0].cols()); }
        if (desc == "DUP") { trans->configure(images.size()); }
        trans->load(rangesList.back(), rac);
        rangesList.push_back(trans->meta(images, rangesList.back()));
        transforms.push_back(trans);
    }
    if (tcount==0) v_printf(4,"none\n"); else v_printf(4,"\n");
    const ColorRanges* ranges = rangesList.back();
    grey.clear();
    for (int p = 0; p < ranges->numPlanes(); p++) grey.push_back((ranges->min(p)+ranges->max(p))/2);

    pixels_todo = width*height*ranges->numPlanes()/scale/scale;

    for (int p = 0; p < ranges->numPlanes(); p++) {
      v_printf(7,"Plane %i: %i..%i\n",p,ranges->min(p),ranges->max(p));
    }

    for (int p = 0; p < ranges->numPlanes(); p++) {
        if (ranges->min(p) >= ranges->max(p)) {
             v_printf(4,"Constant plane %i at color value %i\n",p,ranges->min(p));
             //for (ColorVal_intern& x : image(p).data) x=ranges->min(p);
            for (int fr = 0; fr < numFrames; fr++)
            for (uint32_t r=0; r<images[fr].rows(); r++) {
              for (uint32_t c=0; c<images[fr].cols(); c++) {
                images[fr].set(p,r,c,ranges->min(p));
              }
            }
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
    if (mbits >10) bits=18;
    if (mbits > bits) { printf("OOPS: %i > %i\n",mbits,bits); return false;}


    std::vector<Tree> forest(ranges->numPlanes(), Tree());

    int roughZL = 0;
    if (encoding == 2) {
      roughZL = images[0].zooms() - NB_NOLEARN_ZOOMS-1;
      if (roughZL < 0) roughZL = 0;
//      v_printf(2,"Decoding rough data\n");
      if (bits==10) flif_decode_FLIF2_pass<RacIn<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn<IO>, 10> >(rac, images, ranges, forest, images[0].zooms(), roughZL+1, 100, scale);
      else flif_decode_FLIF2_pass<RacIn<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn<IO>, 18> >(rac, images, ranges, forest, images[0].zooms(), roughZL+1, 100, scale);
    }
    if (encoding == 2 && quality <= 0) {
      v_printf(3,"Not decoding MANIAC tree\n");
    } else {
      v_printf(3,"Decoded header + rough data. Decoding MANIAC tree.\n");
      flif_decode_tree<FLIFBitChanceTree, RacIn<IO>>(rac, ranges, forest, encoding);
    }
//    if (encoding == 1 || quality > 0) {
      switch(encoding) {
        case 1: v_printf(3,"Decoding data (scanlines)\n");
                if (bits==10) flif_decode_scanlines_pass<RacIn<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn<IO>, 10> >(rac, images, ranges, forest);
                else flif_decode_scanlines_pass<RacIn<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn<IO>, 18> >(rac, images, ranges, forest);
                break;
        case 2: v_printf(3,"Decoding data (FLIF2)\n");
                if (bits==10) flif_decode_FLIF2_pass<RacIn<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn<IO>, 10> >(rac, images, ranges, forest, roughZL, 0, quality, scale);
                else flif_decode_FLIF2_pass<RacIn<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn<IO>, 18> >(rac, images, ranges, forest, roughZL, 0, quality, scale);
                break;
      }
//    }
    if (numFrames==1)
      v_printf(2,"\rDecoding done, %li bytes for %ux%u pixels (%.4fbpp)   \n",rac.ftell(), images[0].cols()/scale, images[0].rows()/scale, 1.0*rac.ftell()/images[0].rows()/images[0].cols()/scale/scale);
    else
      v_printf(2,"\rDecoding done, %li bytes for %i frames of %ux%u pixels (%.4fbpp)   \n",rac.ftell(), numFrames, images[0].cols()/scale, images[0].rows()/scale, 1.0*rac.ftell()/numFrames/images[0].rows()/images[0].cols()/scale/scale);


    if (quality==100 && scale==1) {
      uint32_t checksum = images[0].checksum();
      v_printf(8,"Computed checksum: %X\n", checksum);
      uint32_t checksum2 = metaCoder.read_int(0, 0xFFFF);
      checksum2 *= 0x10000;
      checksum2 += metaCoder.read_int(0, 0xFFFF);
      v_printf(8,"Read checksum: %X\n", checksum2);
      if (checksum != checksum2) v_printf(1,"\nCORRUPTION DETECTED! (partial file?)\n\n");
      else v_printf(2,"Image decoded, checksum verified.\n");
    } else {
      v_printf(2,"Not checking checksum, lossy partial decoding was chosen.\n");
    }

    for (int i=transforms.size()-1; i>=0; i--) {
        transforms[i]->invData(images);
        delete transforms[i];
    }
    transforms.clear();


    for (unsigned int i=0; i<rangesList.size(); i++) {
        delete rangesList[i];
    }
    rangesList.clear();

    io.close();
    return true;
}


template bool flif_decode(FileIO io, Images &images, int quality, int scale);
