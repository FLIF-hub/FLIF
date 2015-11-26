#include "config.h"
#ifdef HAS_ENCODER
#include <string>
#include <string.h>

#include "maniac/rac.hpp"
#include "maniac/compound.hpp"
#include "maniac/util.hpp"

#include "flif_config.h"
#include "image/color_range.hpp"
#include "transform/factory.hpp"


#include "common.hpp"
#include "fileio.hpp"

using namespace maniac::util;

template<typename RAC> void static write_name(RAC& rac, std::string desc, uint8_t &transform_l)
{
    int nb = transform_l;
    while (nb <= MAX_TRANSFORM) {
        if (transforms[nb] == desc) break;
        nb++;
    }
    if (transforms[nb] != desc) { e_printf("ERROR: Invalid transform: '%s'\n",desc.c_str()); return;}
    UniformSymbolCoder<RAC> coder(rac);
    coder.write_int(0, MAX_TRANSFORM-transform_l, nb-transform_l);
    transform_l = nb+1;
}

// alphazero = true: image has alpha plane and A=0 implies RGB(YIQ) is irrelevant
// alphazero = false: image either has no alpha plane, or A=0 has no special meaning
// FRA = true: image has FRA plane (animation with lookback)
template<typename IO, typename Rac, typename Coder>
void flif_encode_scanlines_inner(IO& io, Rac& rac, std::vector<Coder> &coders, const Images &images, const ColorRanges *ranges) {
    ColorVal min,max;
    long fs = io.ftell();
    long pixels = images[0].cols()*images[0].rows()*images.size();
    const int nump = images[0].numPlanes();
    const bool alphazero = (nump>3 && images[0].alpha_zero_special);
    const bool FRA = (nump == 5);
    for (int k=0,i=0; k < 5; k++) {
        int p=PLANE_ORDERING[k];
        if (p>=nump) continue;
        i++;
        if (ranges->min(p) >= ranges->max(p)) continue;
        Properties properties((nump>3?NB_PROPERTIES_scanlinesA[p]:NB_PROPERTIES_scanlines[p]));
        v_printf(2,"\r%i%% done [%i/%i] ENC[%ux%u]    ",(int)(100*pixels_done/pixels_todo),i,nump,images[0].cols(),images[0].rows());
        pixels_done += images[0].cols()*images[0].rows();
        for (uint32_t r = 0; r < images[0].rows(); r++) {
            for (int fr=0; fr< (int)images.size(); fr++) {
              const Image& image = images[fr];
              if (image.seen_before >= 0) continue;
              uint32_t begin=image.col_begin[r], end=image.col_end[r];
              for (uint32_t c = begin; c < end; c++) {
                if (alphazero && p<3 && image(3,r,c) == 0) continue;
                if (FRA && p<4 && image(4,r,c) > 0) continue;
                ColorVal guess = predict_and_calcProps_scanlines(properties,ranges,image,p,r,c,min,max);
                ColorVal curr = image(p,r,c);
                assert(p != 3 || curr >= -fr);
                if (FRA && p==4 && max > fr) max = fr;
                coders[p].write_int(properties, min - guess, max - guess, curr - guess);
              }
            }
        }
        long nfs = io.ftell();
        if (nfs-fs > 0) {
           v_printf(3,"filesize : %li (+%li for %li pixels, %f bpp)", nfs, nfs-fs, pixels, 8.0*(nfs-fs)/pixels );
           v_printf(4,"\n");
        }
        fs = nfs;
    }
}

template<typename IO, typename Rac, typename Coder>
void flif_encode_scanlines_pass(IO& io, Rac &rac, const Images &images, const ColorRanges *ranges, std::vector<Tree> &forest,
                                int repeats, int divisor=CONTEXT_TREE_COUNT_DIV, int min_size=CONTEXT_TREE_MIN_SUBTREE_SIZE, int split_threshold=CONTEXT_TREE_SPLIT_THRESHOLD, int cutoff = 2, int alpha = 0xFFFFFFFF / 19) {
    std::vector<Coder> coders;
    coders.reserve(ranges->numPlanes());

    for (int p = 0; p < ranges->numPlanes(); p++) {
        Ranges propRanges;
        initPropRanges_scanlines(propRanges, *ranges, p);
        coders.emplace_back(rac, propRanges, forest[p], split_threshold, cutoff, alpha);
    }

    while(repeats-- > 0) {
     flif_encode_scanlines_inner<IO,Rac,Coder>(io, rac, coders, images, ranges);
    }

    for (int p = 0; p < ranges->numPlanes(); p++) {
        coders[p].simplify(divisor, min_size);
    }
}

template<typename IO, typename Rac, typename Coder>
void flif_encode_FLIF2_inner(IO& io, Rac& rac, std::vector<Coder> &coders, const Images &images,
                             const ColorRanges *ranges, const int beginZL, const int endZL) {
    ColorVal min,max;
    const int nump = images[0].numPlanes();
    const bool alphazero = (nump>3 && images[0].alpha_zero_special);
    const bool FRA = (nump == 5);
    long fs = io.ftell();
    for (int i = 0; i < plane_zoomlevels(images[0], beginZL, endZL); i++) {
      std::pair<int, int> pzl = plane_zoomlevel(images[0], beginZL, endZL, i);
      int p = pzl.first;
      int z = pzl.second;
      if (ranges->min(p) >= ranges->max(p)) continue;
      if (endZL==0) {
          v_printf(2,"\r%i%% done [%i/%i] ENC[%i,%ux%u]  ",(int) (100*pixels_done/pixels_todo),i,plane_zoomlevels(images[0], beginZL, endZL)-1,p,images[0].cols(z),images[0].rows(z));
      }
      pixels_done += (images[0].cols(z)/(z%2==0?1:2))*(images[0].rows(z)/(z%2==0?2:1));
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
                    if (alphazero && p<3 && image(3,z,r,c) == 0) continue;
                    if (FRA && p<4 && image(4,z,r,c) > 0) continue;
                    ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max);
                    ColorVal curr = image(p,z,r,c);
                    if (FRA && p==4 && max > fr) max = fr;
                    assert (curr <= max); assert (curr >= min);
                    coders[p].write_int(properties, min - guess, max - guess, curr - guess);
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
                    if (alphazero && p<3 && image(3,z,r,c) == 0) continue;
                    if (FRA && p<4 && image(4,z,r,c) > 0) continue;
                    ColorVal guess = predict_and_calcProps(properties,ranges,image,z,p,r,c,min,max);
                    ColorVal curr = image(p,z,r,c);
                    if (FRA && p==4 && max > fr) max = fr;
                    assert (curr <= max); assert (curr >= min);
                    coders[p].write_int(properties, min - guess, max - guess, curr - guess);
              }
            }
          }
      }
      if (endZL==0 && io.ftell()>fs) {
          v_printf(3,"    wrote %li bytes    ", io.ftell());
          v_printf(5,"\n");
          fs = io.ftell();
      }
    }
}

template<typename IO, typename Rac, typename Coder>
void flif_encode_FLIF2_pass(IO& io, Rac &rac, const Images &images, const ColorRanges *ranges, std::vector<Tree> &forest, const int beginZL, const int endZL,
                            int repeats, int divisor=CONTEXT_TREE_COUNT_DIV, int min_size=CONTEXT_TREE_MIN_SUBTREE_SIZE, int split_threshold=CONTEXT_TREE_SPLIT_THRESHOLD, int cutoff = 2, int alpha = 0xFFFFFFFF / 19) {
    std::vector<Coder> coders;
    coders.reserve(ranges->numPlanes());
    for (int p = 0; p < ranges->numPlanes(); p++) {
        Ranges propRanges;
        initPropRanges(propRanges, *ranges, p);
        coders.emplace_back(rac, propRanges, forest[p], split_threshold, cutoff, alpha);
    }

    for (const Image& image : images)
    if (beginZL == image.zooms()) {
      // special case: very left top pixel must be written first to get it all started
      SimpleSymbolCoder<FLIFBitChanceMeta, Rac, 24> metaCoder(rac);
      for (int p = 0; p < image.numPlanes(); p++) {
        ColorVal curr = image(p,0,0);
        if (ranges->min(p) < ranges->max(p)) {
            metaCoder.write_int(ranges->min(p), ranges->max(p), curr);
            pixels_done++;
        }
      }
    }
    while(repeats-- > 0) {
     flif_encode_FLIF2_inner<IO,Rac,Coder>(io, rac, coders, images, ranges, beginZL, endZL);
    }
    for (int p = 0; p < images[0].numPlanes(); p++) {
        coders[p].simplify(divisor, min_size);
    }
}

void flif_encode_FLIF2_interpol_zero_alpha(Images &images, const ColorRanges *, const int beginZL, const int endZL)
{
    for (Image& image : images) {
     if (image(3,0,0) == 0) {
        image.set(0,0,0,grey[0]);
        image.set(1,0,0,grey[1]);
        image.set(2,0,0,grey[2]);
     }
     for (int i = 0; i < plane_zoomlevels(image, beginZL, endZL); i++) {
      std::pair<int, int> pzl = plane_zoomlevel(image, beginZL, endZL, i);
      int p = pzl.first;
      int z = pzl.second;
      if (p >= 3) continue;
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
    }
//    v_printf(2,"\n");
}

void flif_encode_scanlines_interpol_zero_alpha(Images &images, const ColorRanges *ranges){
    int nump = images[0].numPlanes();
    if (nump > 3)
    for (Image& image : images)
    for (int p = 0; p < 3; p++) {
//        if (ranges->min(p) >= ranges->max(p)) continue;  // nope, gives problem with fully A=0 image
        for (uint32_t r = 0; r < image.rows(); r++) {
            for (uint32_t c = 0; c < image.cols(); c++) {
                if (image(3,r,c) == 0) {
                    image.set(p,r,c, predict(image,p,r,c));
                }
            }
        }
    }
}


template<typename IO, typename BitChance, typename Rac> void flif_encode_tree(IO& io, Rac &rac, const ColorRanges *ranges, const std::vector<Tree> &forest, const flifEncoding encoding)
{
    for (int p = 0; p < ranges->numPlanes(); p++) {
        Ranges propRanges;
        if (encoding==flifEncoding::nonInterlaced) initPropRanges_scanlines(propRanges, *ranges, p);
        else initPropRanges(propRanges, *ranges, p);
        MetaPropertySymbolCoder<BitChance, Rac> metacoder(rac, propRanges);
//        forest[p].print(stdout);
        if (ranges->min(p)<ranges->max(p))
        metacoder.write_tree(forest[p]);
    }
}
template <int bits, typename IO>
void flif_encode_main(RacOut<IO>& rac, IO& io, Images &images, flifEncoding encoding,
                 int learn_repeats, const ColorRanges *ranges,
                 int divisor=CONTEXT_TREE_COUNT_DIV, int min_size=CONTEXT_TREE_MIN_SUBTREE_SIZE, int split_threshold=CONTEXT_TREE_SPLIT_THRESHOLD, int cutoff = 2, int alpha = 0xFFFFFFFF / 19) {

    Image& image=images[0];

    pixels_todo = (int64_t)image.rows()*image.cols()*ranges->numPlanes()*(learn_repeats+1);
    pixels_done = 0;

    // two passes
    std::vector<Tree> forest(ranges->numPlanes(), Tree());
    RacDummy dummy;


    long fs = io.ftell();

    int roughZL = 0;
    if (encoding == flifEncoding::interlaced) {
      roughZL = image.zooms() - NB_NOLEARN_ZOOMS-1;
      if (roughZL < 0) roughZL = 0;
      //v_printf(2,"Encoding rough data\n");
      flif_encode_FLIF2_pass<IO, RacOut<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut<IO>, bits> >(io, rac, images, ranges, forest, image.zooms(), roughZL+1, 1, divisor, min_size, split_threshold, cutoff, alpha);
    }

    //v_printf(2,"Encoding data (pass 1)\n");
    if (learn_repeats>1) v_printf(3,"Learning a MANIAC tree. Iterating %i times.\n",learn_repeats);
    switch(encoding) {
        case flifEncoding::nonInterlaced:
           flif_encode_scanlines_pass<IO, RacDummy, PropertySymbolCoder<FLIFBitChancePass1, RacDummy, bits> >(io, dummy, images, ranges, forest, learn_repeats, divisor, min_size, split_threshold, cutoff, alpha);
           break;
        case flifEncoding::interlaced:
           flif_encode_FLIF2_pass<IO, RacDummy, PropertySymbolCoder<FLIFBitChancePass1, RacDummy, bits> >(io, dummy, images, ranges, forest, roughZL, 0, learn_repeats, divisor, min_size, split_threshold, cutoff, alpha);
           break;
    }
    v_printf(3,"\rHeader: %li bytes.", fs);
    if (encoding==flifEncoding::interlaced) v_printf(3," Rough data: %li bytes.", io.ftell()-fs);
    fflush(stdout);

    //v_printf(2,"Encoding tree\n");
    fs = io.ftell();
    flif_encode_tree<IO, FLIFBitChanceTree, RacOut<IO>>(io, rac, ranges, forest, encoding);
    v_printf(3," MANIAC tree: %li bytes.\n", io.ftell()-fs);
    //v_printf(2,"Encoding data (pass 2)\n");
    switch(encoding) {
        case flifEncoding::nonInterlaced:
           flif_encode_scanlines_pass<IO, RacOut<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut<IO>, bits> >(io, rac, images, ranges, forest, 1, 0, 0, 0, cutoff, alpha);
           break;
        case flifEncoding::interlaced:
           flif_encode_FLIF2_pass<IO, RacOut<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacOut<IO>, bits> >(io, rac, images, ranges, forest, roughZL, 0, 1, 0, 0, 0, cutoff, alpha);
           break;
    }

}

template <typename IO>
bool flif_encode(IO& io, Images &images, std::vector<std::string> transDesc, flifEncoding encoding,
                 int learn_repeats, int acb, int frame_delay, int palette_size, int lookback,
                 int divisor=CONTEXT_TREE_COUNT_DIV, int min_size=CONTEXT_TREE_MIN_SUBTREE_SIZE, int split_threshold=CONTEXT_TREE_SPLIT_THRESHOLD, int cutoff=2, int alpha=19) {

    io.fputs("FLIF");  // bytes 1-4 are fixed magic
    int numPlanes = images[0].numPlanes();
    int numFrames = images.size();

    // byte 5 encodes color type, interlacing, animation
    // 128 64 32 16 8 4 2 1
    //                                                Gray8    RGB24    RGBA32    Gray16   RGB48    RGBA64
    //      0  1  1           = scanlines, still     (FLIF11,  FLIF31,  FLIF41,   FLIF12,  FLIF32,  FLIF42)
    //      1  0  0           = interlaced, still    (FLIFA1,  FLIFC1,  FLIFD1,   FLIFA2,  FLIFC2,  FLIFD2)
    //      1  0  1           = scanlines, animated  (FLIFQx1, FLIFSx1, FLIFTx1,  FLIFQx2, FLIFSx2, FLIFTx2)   x=nb_frames
    //      1  1  0           = interlaced, animated (FLIFax1, FLIFcx1, FLIFdx1,  FLIFax2, FLIFcx2, FLIFdx2)   x=nb_frames
    //                0 0 1   = grayscale (1 plane)
    //                0 1 1   = RGB (3 planes)
    //                1 0 0   = RGBA (4 planes)       (grayscale + alpha is encoded as RGBA to keep the number of cases low)
    //   0          0         (two spare bits, reserved for future use)
    int c=' '+16*(static_cast<uint8_t>(encoding))+numPlanes;
    if (numFrames>1) c += 32;
    io.fputc(c);

    // for animations: byte 6 = number of frames (or bytes 7 and 8 if nb_frames >= 255)
    if (numFrames>1) {
        if (numFrames<255) io.fputc((char)numFrames);
        else if (numFrames<0xffff) {
            io.fputc(0xff);
            io.fputc(numFrames >> 8);
            io.fputc(numFrames & 0xff);
        } else {
            e_printf("Too many frames!\n");
        }
    }

    // next byte (byte 6 for still images, byte 7 or 9 for animations) encodes the bit depth:
    // '1' for 8 bits (1 byte) per channel, '2' for 16 bits (2 bytes) per channel, '0' for custom bit depth

    c='1';
    for (int p = 0; p < numPlanes; p++) {if (images[0].max(p) != 255) c='2';}
    if (c=='2') {for (int p = 0; p < numPlanes; p++) {if (images[0].max(p) != 65535) c='0';}}
    io.fputc(c);

    Image& image = images[0];
    assert(image.cols() <= 0xFFFF);
    // next 4 bytes (bytes 7-10 for still images, bytes 8-11 or 10-13 for animations) encode the width and height:
    // first the width (as a big-endian unsigned 16 bit number), then the height
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
    int alphazero=0;
    if (numPlanes > 3) {
        if (images[0].alpha_zero_special) alphazero=1;
        if (alphazero) metaCoder.write_int(0,1,1);
        else metaCoder.write_int(0,1,0);
        if (!alphazero) v_printf(3, ", store RGB at A=0");
    }
    v_printf(3,"\n");
    if (numFrames>1) {
        metaCoder.write_int(0, 100, 0); // repeats (0=infinite)
        for (int i=0; i<numFrames; i++) {
           metaCoder.write_int(0, 60000, images[i].frame_delay); // time in ms between frames
        }
    }
//    v_printf(2,"Header: %li bytes.\n", ftell(f));

    metaCoder.write_int(1,128,cutoff);
    metaCoder.write_int(4,128,alpha);
    alpha = 0xFFFFFFFF/alpha;

    std::vector<const ColorRanges*> rangesList;
    std::vector<Transform<IO>*> transforms;
    rangesList.push_back(getRanges(image));
    int tcount=0;
    v_printf(4,"Transforms: ");
    uint8_t transform_l=0;

    for (unsigned int i=0; i<transDesc.size(); i++) {
        Transform<IO> *trans = create_transform<IO>(transDesc[i]);
        if (transDesc[i] == "PLT" || transDesc[i] == "PLA") trans->configure(palette_size);
        if (transDesc[i] == "FRA") trans->configure(lookback);
        if (!trans->init(rangesList.back()) || 
            (!trans->process(rangesList.back(), images)
              && !(acb==1 && transDesc[i] == "ACB" && (v_printf(4,", forced_"), (tcount=0), true) ))) {
            //e_printf( "Transform '%s' failed\n", transDesc[i].c_str());
        } else {
            if (tcount++ > 0) v_printf(4,", ");
            v_printf(4,"%s", transDesc[i].c_str());
            fflush(stdout);
            rac.write_bit(true);
            write_name(rac, transDesc[i], transform_l);
            trans->save(rangesList.back(), rac);
            fflush(stdout);
            rangesList.push_back(trans->meta(images, rangesList.back()));
            trans->data(images);
        }
        delete trans;
    }
    if (tcount==0) v_printf(4,"none\n"); else v_printf(4,"\n");
    rac.write_bit(false);
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
#ifdef SUPPORT_HDR
    if (mbits >10) bits=18;
#endif
    if (mbits > bits) { e_printf("OOPS: this FLIF only supports 8-bit RGBA (not compiled with SUPPORT_HDR)\n"); return false;}

    if (alphazero && ranges->numPlanes() > 3 && ranges->min(3) <= 0) {
      v_printf(4,"Replacing fully transparent pixels with predicted pixel values at the other planes\n");
      switch(encoding) {
        case flifEncoding::nonInterlaced: flif_encode_scanlines_interpol_zero_alpha(images, ranges); break;
        case flifEncoding::interlaced: flif_encode_FLIF2_interpol_zero_alpha(images, ranges, image.zooms(), 0); break;
      }
    }
    for (int p = 0; p < ranges->numPlanes(); p++) {
        if (ranges->min(p) >= ranges->max(p)) {
            v_printf(4,"Constant plane %i at color value %i\n",p,ranges->min(p));
            pixels_todo -= (int64_t)image.rows()*image.cols()*(learn_repeats+1);
            for (int fr = 0; fr < numFrames; fr++)
                images[fr].make_constant_plane(p,ranges->min(p));
        }
    }
    // not computing checksum until after transformations and potential zero-alpha changes
    const uint32_t checksum = image.checksum();


    if (bits ==10) {
      flif_encode_main<10>(rac, io, images, encoding, learn_repeats, ranges, divisor, min_size, split_threshold, cutoff, alpha);
#ifdef SUPPORT_HDR
    } else {
      flif_encode_main<18>(rac, io, images, encoding, learn_repeats, ranges, divisor, min_size, split_threshold, cutoff, alpha);
#endif
    }

    //v_printf(2,"Writing checksum: %X\n", checksum);
    metaCoder.write_int(0, 0xFFFF, checksum / 0x10000);
    metaCoder.write_int(0, 0xFFFF, checksum & 0xFFFF);
    rac.flush();
    io.flush();

    if (numFrames==1)
      v_printf(2,"\rEncoding done, %li bytes for %ux%u pixels (%.4fbpp)   \n",io.ftell(), images[0].cols(), images[0].rows(), 8.0*io.ftell()/images[0].rows()/images[0].cols());
    else
      v_printf(2,"\rEncoding done, %li bytes for %i frames of %ux%u pixels (%.4fbpp)   \n",io.ftell(), numFrames, images[0].cols(), images[0].rows(), 8.0*io.ftell()/numFrames/images[0].rows()/images[0].cols());

//    images[0].save("debug.pam");

    for (int i=(int)transforms.size()-1; i>=0; i--) {
        delete transforms[i];
    }
    transforms.clear();
    for (unsigned int i=0; i<rangesList.size(); i++) {
        delete rangesList[i];
    }
    rangesList.clear();
    return true;
}


template bool flif_encode(FileIO& io, Images &images, std::vector<std::string> transDesc, flifEncoding encoding, int learn_repeats, int acb, int frame_delay, int palette_size, int lookback, int divisor, int min_size, int split_threshold, int cutoff, int alpha);
template bool flif_encode(BlobIO& io, Images &images, std::vector<std::string> transDesc, flifEncoding encoding, int learn_repeats, int acb, int frame_delay, int palette_size, int lookback, int divisor, int min_size, int split_threshold, int cutoff, int alpha);

#endif
