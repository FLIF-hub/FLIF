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

template<typename RAC> void static write_name(RAC& rac, std::string desc)
{
    int nb=0;
    while (nb <= MAX_TRANSFORM) {
        if (transforms[nb] == desc) break;
        nb++;
    }
    if (transforms[nb] != desc) { fprintf(stderr,"ERROR: Unknown transform description string!\n"); return;}
    UniformSymbolCoder<RAC> coder(rac);
    coder.write_int(0, MAX_TRANSFORM, nb);
}


template<typename Rac, typename Coder> void flif_encode_scanlines_inner(Rac rac, std::vector<Coder*> &coders, const Images &images, const ColorRanges *ranges)
{
    ColorVal min,max;
    long fs = rac.ftell();
    long pixels = images[0].cols()*images[0].rows()*images.size();
    int nump = images[0].numPlanes();
    int beginp = (nump>3 ? 3 : 0);
    for (int p = beginp, i=0; i++ < nump; p = (p+1)%nump) {
        Properties properties((nump>3?NB_PROPERTIES_scanlinesA[p]:NB_PROPERTIES_scanlines[p]));
        v_printf(2,"\r%i%% done [%i/%i] ENC[%ux%u]    ",(int)(100*pixels_done/pixels_todo),i,nump,images[0].cols(),images[0].rows());
        pixels_done += images[0].cols()*images[0].rows();
        if (ranges->min(p) >= ranges->max(p)) continue;
        for (uint32_t r = 0; r < images[0].rows(); r++) {
            for (int fr=0; fr< (int)images.size(); fr++) {
              const Image& image = images[fr];
              if (image.seen_before >= 0) continue;
              uint32_t begin=image.col_begin[r], end=image.col_end[r];
              for (uint32_t c = begin; c < end; c++) {
                if (nump>3 && p<3 && image(3,r,c) <= 0) continue;
                ColorVal guess = predict_and_calcProps_scanlines(properties,ranges,image,p,r,c,min,max);
                ColorVal curr = image(p,r,c);
                assert(p != 3 || curr >= -fr);
                if (p==3 && min < -fr) min = -fr;
                coders[p]->write_int(properties, min - guess, max - guess, curr - guess);
              }
            }
        }
        long nfs = rac.ftell();
        if (nfs-fs > 0) {
           v_printf(3,"filesize : %li (+%li for %li pixels, %f bpp)", nfs, nfs-fs, pixels, 8.0*(nfs-fs)/pixels );
           v_printf(4,"\n");
        }
        fs = nfs;
    }
}

template<typename Rac, typename Coder> void flif_encode_scanlines_pass(Rac &rac, const Images &images, const ColorRanges *ranges, std::vector<Tree> &forest, int repeats)
{
    std::vector<Coder*> coders;

    for (int p = 0; p < ranges->numPlanes(); p++) {
        Ranges propRanges;
        initPropRanges_scanlines(propRanges, *ranges, p);
        coders.push_back(new Coder(rac, propRanges, forest[p]));
    }

    while(repeats-- > 0) {
     flif_encode_scanlines_inner(rac, coders, images, ranges);
    }

    for (int p = 0; p < ranges->numPlanes(); p++) {
        coders[p]->simplify();
    }

    for (int p = 0; p < ranges->numPlanes(); p++) {
#ifdef STATS
        indent(0); v_printf(2,"Plane %i\n", p);
        coders[p]->info(0+1);
#endif
        delete coders[p];
    }
}

template<typename Rac, typename Coder> void flif_encode_FLIF2_inner(Rac rac, std::vector<Coder*> &coders, const Images &images, const ColorRanges *ranges, const int beginZL, const int endZL)
{
    ColorVal min,max;
    int nump = images[0].numPlanes();
    long fs = rac.ftell();
    for (int i = 0; i < plane_zoomlevels(images[0], beginZL, endZL); i++) {
      std::pair<int, int> pzl = plane_zoomlevel(images[0], beginZL, endZL, i);
      int p = pzl.first;
      int z = pzl.second;
      if (endZL==0) {
          v_printf(2,"\r%i%% done [%i/%i] ENC[%i,%ux%u]  ",(int) (100*pixels_done/pixels_todo),i,plane_zoomlevels(images[0], beginZL, endZL)-1,p,images[0].cols(z),images[0].rows(z));
      }
      pixels_done += images[0].cols(z)*images[0].rows(z)/2;
      if (ranges->min(p) >= ranges->max(p)) continue;
      Properties properties((nump>3?NB_PROPERTIESA[p]:NB_PROPERTIES[p]));
      if (z % 2 == 0) {
        // horizontal: scan the odd rows, output pixel values
          for (uint32_t r = 1; r < images[0].rows(z); r += 2) {
            for (int fr=0; fr<(int)images.size(); fr++) {
              const Image& image = images[fr];
              if (image.seen_before >= 0) { continue; }
              uint32_t begin=(image.col_begin[r*image.zoom_rowpixelsize(z)]/image.zoom_colpixelsize(z)),
                         end=(1+(image.col_end[r*image.zoom_rowpixelsize(z)]-1)/image.zoom_colpixelsize(z));
              for (uint32_t c = begin; c < end; c++) {
                    if (nump>3 && p<3 && image(3,z,r,c) <= 0) continue;
                    ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max);
                    ColorVal curr = image(p,z,r,c);
                    if (p==3 && min < -fr) min = -fr;
                    assert (curr <= max); assert (curr >= min);
                    coders[p]->write_int(properties, min - guess, max - guess, curr - guess);
              }
            }
          }
      } else {
        // vertical: scan the odd columns
          for (uint32_t r = 0; r < images[0].rows(z); r++) {
            for (int fr=0; fr<(int)images.size(); fr++) {
              const Image& image = images[fr];
              if (image.seen_before >= 0) { continue; }
              uint32_t begin=(image.col_begin[r*image.zoom_rowpixelsize(z)]/image.zoom_colpixelsize(z)),
                         end=(1+(image.col_end[r*image.zoom_rowpixelsize(z)]-1)/image.zoom_colpixelsize(z))|1;
              if (begin>1 && ((begin&1) ==0)) begin--;
              if (begin==0) begin=1;
              for (uint32_t c = begin; c < end; c+=2) {
                    if (nump>3 && p<3 && image(3,z,r,c) <= 0) continue;
                    ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max);
                    ColorVal curr = image(p,z,r,c);
                    if (p==3 && min < -fr) min = -fr;
                    assert (curr <= max); assert (curr >= min);
                    coders[p]->write_int(properties, min - guess, max - guess, curr - guess);
              }
            }
          }
      }
      if (endZL==0 && rac.ftell()>fs) {
          v_printf(3,"    wrote %li bytes    ", rac.ftell());
          v_printf(5,"\n");
          fs = rac.ftell();
      }
    }
}

template<typename Rac, typename Coder> void flif_encode_FLIF2_pass(Rac &rac, const Images &images, const ColorRanges *ranges, std::vector<Tree> &forest, const int beginZL, const int endZL, int repeats)
{
    std::vector<Coder*> coders;
    for (int p = 0; p < ranges->numPlanes(); p++) {
        Ranges propRanges;
        initPropRanges(propRanges, *ranges, p);
        coders.push_back(new Coder(rac, propRanges, forest[p]));
    }

    for (const Image& image : images)
    if (beginZL == image.zooms()) {
      // special case: very left top pixel must be written first to get it all started
      SimpleSymbolCoder<FLIFBitChanceMeta, Rac, 24> metaCoder(rac);
      for (int p = 0; p < image.numPlanes(); p++) {
        ColorVal curr = image(p,0,0);
        metaCoder.write_int(ranges->min(p), ranges->max(p), curr);
      }
    }
    while(repeats-- > 0) {
     flif_encode_FLIF2_inner(rac, coders, images, ranges, beginZL, endZL);
    }
    for (int p = 0; p < images[0].numPlanes(); p++) {
        coders[p]->simplify();
    }

    for (int p = 0; p < images[0].numPlanes(); p++) {
#ifdef STATS
        indent(0); v_printf(2,"Plane %i\n", p);
        coders[p]->info(0+1);
#endif
        delete coders[p];
    }
}

void flif_encode_FLIF2_interpol_zero_alpha(Images &images, const ColorRanges *ranges, const int beginZL, const int endZL)
{
    for (Image& image : images)
    for (int i = 0; i < plane_zoomlevels(image, beginZL, endZL); i++) {
      std::pair<int, int> pzl = plane_zoomlevel(image, beginZL, endZL, i);
      int p = pzl.first;
      int z = pzl.second;
      if (p == 3) continue;
//      v_printf(2,"[%i] interpol_zero_alpha ",p);
//      fflush(stdout);
      if (z % 2 == 0) {
        // horizontal: scan the odd rows
          for (uint32_t r = 1; r < image.rows(z); r += 2) {
            for (uint32_t c = 0; c < image.cols(z); c++) {
               if (image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c));
            }
          }
      } else {
        // vertical: scan the odd columns
          for (uint32_t r = 0; r < image.rows(z); r++) {
            for (uint32_t c = 1; c < image.cols(z); c += 2) {
               if (image(3,z,r,c) == 0) image.set(p,z,r,c, predict(image,z,p,r,c));
            }
          }
      }
    }
//    v_printf(2,"\n");
}

void flif_encode_scanlines_interpol_zero_alpha(Images &images, const ColorRanges *ranges)
{

    ColorVal min,max;
    int nump = images[0].numPlanes();
    if (nump > 3)
    for (Image& image : images)
    for (int p = 0; p < 3; p++) {
        Properties properties((nump>3?NB_PROPERTIES_scanlinesA[p]:NB_PROPERTIES_scanlines[p]));
        if (ranges->min(p) >= ranges->max(p)) continue;
//          v_printf(2,"[%i] interpol_zero_alpha ",p);
//        fflush(stdout);
        for (uint32_t r = 0; r < image.rows(); r++) {
            for (uint32_t c = 0; c < image.cols(); c++) {
                if (image(3,r,c) == 0) {
                    image.set(p,r,c, predict_and_calcProps_scanlines(properties,ranges,image,p,r,c,min,max));
                }
            }
        }
    }
//    v_printf(2,"\n");
}


template<typename BitChance, typename Rac> void flif_encode_tree(Rac &rac, const ColorRanges *ranges, const std::vector<Tree> &forest, const int encoding)
{
    for (int p = 0; p < ranges->numPlanes(); p++) {
        Ranges propRanges;
        if (encoding==1) initPropRanges_scanlines(propRanges, *ranges, p);
        else initPropRanges(propRanges, *ranges, p);
        MetaPropertySymbolCoder<BitChance, Rac> metacoder(rac, propRanges);
//        forest[p].print(stdout);
        if (ranges->min(p)<ranges->max(p))
        metacoder.write_tree(forest[p]);
    }
}

template <typename IO>
bool flif_encode(IO io, Images &images, std::vector<std::string> transDesc, int encoding, int learn_repeats, int acb, int frame_delay, int palette_size, int lookback) {
    if (encoding < 1 || encoding > 2) { fprintf(stderr,"Unknown encoding: %i\n", encoding); return false;}
    io.fputs("FLIF");
    int numPlanes = images[0].numPlanes();
    int numFrames = images.size();
    char c=' '+16*encoding+numPlanes;
    if (numFrames>1) c += 32;
    io.fputc(c);
    if (numFrames>1) {
        if (numFrames<255) io.fputc((char)numFrames);
        else {
            fprintf(stderr,"Too many frames!\n");
        }
    }
    c='1';
    for (int p = 0; p < numPlanes; p++) {if (images[0].max(p) != 255) c='2';}
    if (c=='2') {for (int p = 0; p < numPlanes; p++) {if (images[0].max(p) != 65535) c='0';}}
    io.fputc(c);

    Image& image = images[0];
    assert(image.cols() <= 0xFFFF);
    io.fputc(image.cols() >> 8);
    io.fputc(image.cols() & 0xFF);
    assert(image.rows() <= 0xFFFF);
    io.fputc(image.rows() >> 8);
    io.fputc(image.rows() & 0xFF);

    RacOut<IO> rac(io);
    SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 24> metaCoder(rac);

    v_printf(3,"Input: %ux%u, channels:", images[0].cols(), images[0].rows());
    for (int p = 0; p < numPlanes; p++) {
        assert(image.min(p) == 0);
        if (c=='0') {
          metaCoder.write_int(1, 16, ilog2(image.max(p)+1));
          v_printf(3," [%i] %i bpp",p,ilog2(image.max(p)+1));
        }
    }
    if (c=='1') v_printf(3," %i, depth: 8 bit",numPlanes);
    if (c=='2') v_printf(3," %i, depth: 16 bit",numPlanes);
    if (numFrames>1) v_printf(3,", frames: %i",numFrames);
    v_printf(3,"\n");
    if (numFrames>1) {
        for (int i=0; i<numFrames; i++) {
           metaCoder.write_int(0, 60000, frame_delay); // time in ms between frames
        }
    }
//    metaCoder.write_int(1, 65536, image.cols());
//    metaCoder.write_int(1, 65536, image.rows());
//    v_printf(2,"Header: %li bytes.\n", ftell(f));

//    v_printf(2,"Header: %li bytes.\n", ftell(f));

    std::vector<const ColorRanges*> rangesList;
    std::vector<Transform<IO>*> transforms;
    rangesList.push_back(getRanges(image));
    int tcount=0;
    v_printf(4,"Transforms: ");
    for (unsigned int i=0; i<transDesc.size(); i++) {
        Transform<IO> *trans = create_transform<IO>(transDesc[i]);
        if (transDesc[i] == "PLT" || transDesc[i] == "PLA") trans->configure(palette_size);
        if (transDesc[i] == "FRA") trans->configure(lookback);
        if (!trans->init(rangesList.back()) || 
            (!trans->process(rangesList.back(), images)
              && !(acb==1 && transDesc[i] == "ACB" && printf(", forced_") && (tcount=0)==0))) {
            //fprintf(stderr, "Transform '%s' failed\n", transDesc[i].c_str());
        } else {
            if (tcount++ > 0) v_printf(4,", ");
            v_printf(4,"%s", transDesc[i].c_str());
            fflush(stdout);
            rac.write(true);
            write_name(rac, transDesc[i]);
            trans->save(rangesList.back(), rac);
            fflush(stdout);
            rangesList.push_back(trans->meta(images, rangesList.back()));
            trans->data(images);
        }
        delete trans;
    }
    if (tcount==0) v_printf(4,"none\n"); else v_printf(4,"\n");
    rac.write(false);
    const ColorRanges* ranges = rangesList.back();
    grey.clear();
    for (int p = 0; p < ranges->numPlanes(); p++) grey.push_back((ranges->min(p)+ranges->max(p))/2);

    for (int p = 0; p < ranges->numPlanes(); p++) {
      v_printf(7,"Plane %i: %i..%i\n",p,ranges->min(p),ranges->max(p));
    }

    int mbits = 0;
    for (int p = 0; p < ranges->numPlanes(); p++) {
        if (ranges->max(p) > ranges->min(p)) {
          int nBits = ilog2((ranges->max(p) - ranges->min(p))*2-1)+1;
          if (nBits > mbits) mbits = nBits;
        }
    }
    int bits = 10; // hardcoding things for 8 bit RGB (which means 9 bit IQ and 10 bit differences)
    if (mbits >10) bits=18;
    if (mbits > bits) { printf("OOPS: %i > %i\n",mbits,bits); return false;}

    pixels_todo = image.rows()*image.cols()*ranges->numPlanes()*(learn_repeats+1);

    // two passes
    std::vector<Tree> forest(ranges->numPlanes(), Tree());
    RacDummy dummy;

    if (ranges->numPlanes() > 3) {
      v_printf(4,"Replacing fully transparent pixels with predicted pixel values at the other planes\n");
      switch(encoding) {
        case 1: flif_encode_scanlines_interpol_zero_alpha(images, ranges); break;
        case 2: flif_encode_FLIF2_interpol_zero_alpha(images, ranges, image.zooms(), 0); break;
      }
    }

    // not computing checksum until after transformations and potential zero-alpha changes
    uint32_t checksum = image.checksum();
    long fs = io.ftell();

    int roughZL = 0;
    if (encoding == 2) {
      roughZL = image.zooms() - NB_NOLEARN_ZOOMS-1;
      if (roughZL < 0) roughZL = 0;
      //v_printf(2,"Encoding rough data\n");
      if (bits==10) flif_encode_FLIF2_pass<RacOut<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut<IO>, 10> >(rac, images, ranges, forest, image.zooms(), roughZL+1, 1);
      else flif_encode_FLIF2_pass<RacOut<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut<IO>, 18> >(rac, images, ranges, forest, image.zooms(), roughZL+1, 1);
    }

    //v_printf(2,"Encoding data (pass 1)\n");
    if (learn_repeats>1) v_printf(3,"Learning a MANIAC tree. Iterating %i times.\n",learn_repeats);
    switch(encoding) {
        case 1:
           if (bits==10) flif_encode_scanlines_pass<RacDummy, PropertySymbolCoder<FLIFBitChancePass1, RacDummy, 10> >(dummy, images, ranges, forest, learn_repeats);
           else flif_encode_scanlines_pass<RacDummy, PropertySymbolCoder<FLIFBitChancePass1, RacDummy, 18> >(dummy, images, ranges, forest, learn_repeats);
           break;
        case 2:
           if (bits==10) flif_encode_FLIF2_pass<RacDummy, PropertySymbolCoder<FLIFBitChancePass1, RacDummy, 10> >(dummy, images, ranges, forest, roughZL, 0, learn_repeats);
           else flif_encode_FLIF2_pass<RacDummy, PropertySymbolCoder<FLIFBitChancePass1, RacDummy, 18> >(dummy, images, ranges, forest, roughZL, 0, learn_repeats);
           break;
    }
    v_printf(3,"\rHeader: %li bytes.", fs);
    if (encoding==2) v_printf(3," Rough data: %li bytes.", io.ftell()-fs);
    fflush(stdout);

    //v_printf(2,"Encoding tree\n");
    fs = io.ftell();
    flif_encode_tree<FLIFBitChanceTree, RacOut<IO>>(rac, ranges, forest, encoding);
    v_printf(3," MANIAC tree: %li bytes.\n", io.ftell()-fs);
    //v_printf(2,"Encoding data (pass 2)\n");
    switch(encoding) {
        case 1:
           if (bits==10) flif_encode_scanlines_pass<RacOut<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut<IO>, 10> >(rac, images, ranges, forest, 1);
           else flif_encode_scanlines_pass<RacOut<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut<IO>, 18> >(rac, images, ranges, forest, 1);
           break;
        case 2:
           if (bits==10) flif_encode_FLIF2_pass<RacOut<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut<IO>, 10> >(rac, images, ranges, forest, roughZL, 0, 1);
           else flif_encode_FLIF2_pass<RacOut<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut<IO>, 18> >(rac, images, ranges, forest, roughZL, 0, 1);
           break;
    }
    if (numFrames==1)
      v_printf(2,"\rEncoding done, %li bytes for %ux%u pixels (%.4fbpp)   \n",io.ftell(), images[0].cols(), images[0].rows(), 1.0*io.ftell()/images[0].rows()/images[0].cols());
    else
      v_printf(2,"\rEncoding done, %li bytes for %i frames of %ux%u pixels (%.4fbpp)   \n",io.ftell(), numFrames, images[0].cols(), images[0].rows(), 1.0*io.ftell()/numFrames/images[0].rows()/images[0].cols());

    //v_printf(2,"Writing checksum: %X\n", checksum);
    metaCoder.write_int(0, 0xFFFF, checksum / 0x10000);
    metaCoder.write_int(0, 0xFFFF, checksum & 0xFFFF);
    rac.flush();
    io.close();

    for (int i=transforms.size()-1; i>=0; i--) {
        delete transforms[i];
    }
    transforms.clear();
    for (unsigned int i=0; i<rangesList.size(); i++) {
        delete rangesList[i];
    }
    rangesList.clear();
    return true;
}


template bool flif_encode(FileIO io, Images &images, std::vector<std::string> transDesc, int encoding, int learn_repeats, int acb, int frame_delay, int palette_size, int lookback);

