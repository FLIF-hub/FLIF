/*
FLIF decoder - Free Lossless Image Format

Copyright 2010-2016, Jon Sneyers & Pieter Wuille

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <string>
#include <string.h>

#include "maniac/rac.hpp"
#include "maniac/compound.hpp"
#include "maniac/util.hpp"

#include "image/color_range.hpp"
#include "transform/factory.hpp"

#include "transform/colorbuckets.hpp"
#include "transform/bounds.hpp"

#include "flif_config.h"

#include "common.hpp"
#include "fileio.hpp"

#include "flif-dec.hpp"

using namespace maniac::util;

template<typename RAC> std::string static read_name(RAC& rac, int &nb) {
    UniformSymbolCoder<RAC> coder(rac);
    nb = coder.read_int(0, MAX_TRANSFORM);
    if (nb>MAX_TRANSFORM) nb=MAX_TRANSFORM;
    return transforms[nb];
}

template<typename Coder, typename plane_t, typename alpha_t>
void flif_decode_scanline_plane(plane_t &plane, Coder &coder, Images &images, const ColorRanges *ranges, alpha_t &alpha, Properties &properties, 
                                const int p, const int fr, const uint32_t r, const ColorVal grey, const ColorVal minP, const bool alphazero, const bool FRA) {
    ColorVal min,max;
    Image& image = images[fr];
    uint32_t begin=0, end=image.cols();
    //if this is a duplicate frame, copy the row from the frame being duplicated
    if (image.seen_before >= 0) { 
        copy_row_range(plane,images[image.seen_before].getPlane(p),r,0,image.cols());
        return;
    }
    //if this is not the first or only frame, fill the beginning of the row before the actual pixel data
    if (fr > 0) {
        //if alphazero is on, fill with a predicted value, otherwise copy pixels from the previous frame
        begin=image.col_begin[r]; end=image.col_end[r];
        if (alphazero && p < 3) {
            for (uint32_t c = 0; c < begin; c++)
                if (alpha.get(r,c) == 0) plane.set(r,c,predictScanlines_plane(plane,r,c, grey));
                else image.set(p,r,c,images[fr-1](p,r,c));
        }
        else if(p!=4) {
            copy_row_range(plane,images[fr - 1].getPlane(p), r, 0, begin);
        }
    } else if (image.numPlanes()>3 && p<3) { begin=0; end=image.cols(); }
    //decode actual pixel data
#if LARGE_BINARY > 0
    if (r > 1 && !FRA && begin == 0 && end > 3) {
      uint32_t c = begin;
      for (; c < 2; c++) {
        if (alphazero && p<3 && alpha.get(r,c) == 0) {plane.set(r,c,predictScanlines_plane(plane,r,c, grey)); continue;}
        ColorVal guess = predict_and_calcProps_scanlines_plane<plane_t,false>(properties,ranges,image,plane,p,r,c,min,max, minP);
        ColorVal curr = coder.read_int(properties, min - guess, max - guess) + guess;
        plane.set(r,c, curr);
      }
      for (; c < end-1; c++) {
        if (alphazero && p<3 && alpha.get(r,c) == 0) {plane.set(r,c,predictScanlines_plane(plane,r,c, grey)); continue;}
        ColorVal guess = predict_and_calcProps_scanlines_plane<plane_t,true>(properties,ranges,image,plane,p,r,c,min,max, minP);
        ColorVal curr = coder.read_int(properties, min - guess, max - guess) + guess;
        plane.set(r,c, curr);
      }
      for (; c < end; c++) {
        if (alphazero && p<3 && alpha.get(r,c) == 0) {plane.set(r,c,predictScanlines_plane(plane,r,c, grey)); continue;}
        ColorVal guess = predict_and_calcProps_scanlines_plane<plane_t,false>(properties,ranges,image,plane,p,r,c,min,max, minP);
        ColorVal curr = coder.read_int(properties, min - guess, max - guess) + guess;
        plane.set(r,c, curr);
      }
    } else
#endif
    {
      for (uint32_t c = begin; c < end; c++) {
        //predict pixel for alphazero and get a previous pixel for FRA
        if (alphazero && p<3 && alpha.get(r,c) == 0) {plane.set(r,c,predictScanlines_plane(plane,r,c, grey)); continue;}
        if (FRA && p<4 && image.getFRA(r,c) > 0) {assert(fr >= image.getFRA(r,c)); plane.set(r,c,images[fr-image.getFRA(r,c)](p,r,c)); continue;}
        //calculate properties and use them to decode the next pixel
        ColorVal guess = predict_and_calcProps_scanlines_plane<plane_t,false>(properties,ranges,image,plane,p,r,c,min,max, minP);
        if (FRA && p==4 && max > fr) max = fr;
        ColorVal curr = coder.read_int(properties, min - guess, max - guess) + guess;
        plane.set(r,c, curr);
      }
    }
    //if this is not the first or only frame, fill the end of the row after the actual pixel data
    if (fr>0) {
        //if alphazero is on, fill with a predicted value, otherwise copy pixels from the previous frame
        if (alphazero && p < 3) {
            for (uint32_t c = end; c < image.cols(); c++)
                if (alpha.get(r,c) == 0) plane.set(r,c,predictScanlines_plane(plane,r,c, grey));
                else image.set(p,r,c,images[fr-1](p,r,c));
        }
        else if(p!=4) { 
            copy_row_range(plane,images[fr - 1].getPlane(p), r, end, image.cols());
        }
    }
}

//TODO use tuples or something to make this less ugly/more generic
template<typename Coder, typename alpha_t>
struct scanline_plane_decoder: public PlaneVisitor {
    Coder &coder; Images &images; const ColorRanges *ranges; Properties &properties; const alpha_t &alpha; const int p, fr; const uint32_t r; const ColorVal grey, minP; const bool alphazero, FRA;
    scanline_plane_decoder(Coder &c, Images &i, const ColorRanges *ra, Properties &prop, const GeneralPlane &al, const int pl, const int f, const uint32_t row, const ColorVal g, const ColorVal m, const bool az, const bool fra) :
        coder(c), images(i), ranges(ra), properties(prop), alpha(static_cast<const alpha_t&>(al)), p(pl), fr(f), r(row), grey(g), minP(m), alphazero(az), FRA(fra) {}

    void visit(Plane<ColorVal_intern_8>   &plane) override {flif_decode_scanline_plane(plane,coder,images,ranges,alpha,properties,p,fr,r,grey,minP,alphazero,FRA);}
    void visit(Plane<ColorVal_intern_16>  &plane) override {flif_decode_scanline_plane(plane,coder,images,ranges,alpha,properties,p,fr,r,grey,minP,alphazero,FRA);}
#ifdef SUPPORT_HDR
    void visit(Plane<ColorVal_intern_16u> &plane) override {flif_decode_scanline_plane(plane,coder,images,ranges,alpha,properties,p,fr,r,grey,minP,alphazero,FRA);}
    void visit(Plane<ColorVal_intern_32>  &plane) override {flif_decode_scanline_plane(plane,coder,images,ranges,alpha,properties,p,fr,r,grey,minP,alphazero,FRA);}
#endif
//    void visit(ConstantPlane              &plane) override {flif_decode_scanline_plane(plane,coder,images,ranges,alpha,properties,p,fr,r,grey,minP,alphazero,FRA);}
};

template<typename IO, typename Rac, typename Coder>
bool flif_decode_scanlines_inner(IO &io, Rac &rac, std::vector<Coder> &coders, Images &images, const ColorRanges *ranges, int quality,
                                 std::vector<Transform<IO>*> &transforms, uint32_t (*callback)(int32_t,int64_t), Images &partial_images) {
    const int nump = images[0].numPlanes();
    const bool alphazero = images[0].alpha_zero_special;
    const bool FRA = (nump == 5);
    if (callback || quality<100) {
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

    const std::vector<ColorVal> greys = computeGreys(ranges);

    for (int k=0,i=0; k < 5; k++) {
        int p=PLANE_ORDERING[k];
        if (p>=nump) continue;
        i++;
        Properties properties((nump>3?NB_PROPERTIES_scanlinesA[p]:NB_PROPERTIES_scanlines[p]));
        if ((100*pixels_done > quality*pixels_todo)) {
          v_printf(5,"%lu subpixels done, %lu subpixels todo, quality target %i%% reached (%i%%)\n",(long unsigned)pixels_done,(long unsigned)pixels_todo,(int)quality,(int)(100*pixels_done/pixels_todo));
          return false;
        }
        if (ranges->min(p) < ranges->max(p)) {
          const ColorVal minP = ranges->min(p);
          v_printf_tty(2,"\r%i%% done [%i/%i] DEC[%ux%u]    ",(int)(100*pixels_done/pixels_todo),i,nump,images[0].cols(),images[0].rows());
          v_printf_tty(4,"\n");
          pixels_done += images[0].cols()*images[0].rows();
          for (uint32_t r = 0; r < images[0].rows(); r++) {
            if (images[0].cols() == 0) return false; // decode aborted
            for (int fr=0; fr< (int)images.size(); fr++) {
                Image &image = images[fr];
                GeneralPlane &plane = image.getPlane(p);
                ConstantPlane null_alpha(1);
                GeneralPlane &alpha = nump > 3 ? image.getPlane(3) : null_alpha;
                if (alpha.is_constant()) {
                    scanline_plane_decoder<Coder,ConstantPlane> decoder(coders[p],images,ranges,properties,alpha,p,fr,r,greys[p],minP,alphazero,FRA);
                    plane.accept_visitor(decoder);
                } else if (image.getDepth() <= 8) {
                    scanline_plane_decoder<Coder,Plane<ColorVal_intern_8>> decoder(coders[p],images,ranges,properties,alpha,p,fr,r,greys[p],minP,alphazero,FRA);
                    plane.accept_visitor(decoder);
#ifdef SUPPORT_HDR
                } else {
                    scanline_plane_decoder<Coder,Plane<ColorVal_intern_16u>> decoder(coders[p],images,ranges,properties,alpha,p,fr,r,greys[p],minP,alphazero,FRA);
                    plane.accept_visitor(decoder);
#endif
                }
            }
          }
          int qual = 10000*pixels_done/pixels_todo;
          if (callback && p != 4 && qual >= progressive_qual_target) {
            for (unsigned int n=0; n < images.size(); n++) partial_images[n] = images[n].clone(); // make a copy to work with
            for (int i=transforms.size()-1; i>=0; i--) if (transforms[i]->undo_redo_during_decode()) transforms[i]->invData(partial_images);
            progressive_qual_shown = qual;
            progressive_qual_target = callback(qual,io.ftell());
            if (qual >= progressive_qual_target) return false;
          }
        }
    }
    return true;
}

template<typename IO, typename Rac, typename Coder>
bool flif_decode_scanlines_pass(IO& io, Rac &rac, Images &images, const ColorRanges *ranges, std::vector<Tree> &forest, int quality,
                                std::vector<Transform<IO>*> &transforms, uint32_t (*callback)(int32_t,int64_t), Images &partial_images,
                                int cutoff = 2, int alpha = 0xFFFFFFFF / 19) {
    std::vector<Coder> coders;
    coders.reserve(images[0].numPlanes());
    for (int p = 0; p < images[0].numPlanes(); p++) {
        Ranges propRanges;
        initPropRanges_scanlines(propRanges, *ranges, p);
        coders.emplace_back(rac, propRanges, forest[p], 0, cutoff, alpha);
    }
    return flif_decode_scanlines_inner<IO,Rac,Coder>(io, rac, coders, images, ranges, quality, transforms, callback, partial_images);
}

// interpolate rest of the image
// used when decoding lossy
template<typename IO>
void flif_decode_FLIF2_inner_interpol(Images &images, const ColorRanges *ranges, const int P,
                                      const int beginZL, const int endZL, const int32_t R, const int scale, std::vector<int> zoomlevels, std::vector<Transform<IO>*> &transforms) {

    // finish the zoomlevel we were working on
    if (R>=0) {
      int z = zoomlevels[P];
      int p = P;
      v_printf_tty(2,"\nINTERPOLATE_REMAINING[%i,%ux%u]                 ",p,images[0].cols(z),images[0].rows(z));
      v_printf_tty(5,"\n");
      if (z >= endZL) zoomlevels[P]--;
      if (z % 2 == 0) {
        // horizontal: scan the odd rows
          for (uint32_t r = (R>=0 ? R : 1); r < images[0].rows(z); r += 2) {
            for (Image& image : images) {
              if (image.palette == false) {
               for (uint32_t c = 0; c < image.cols(z); c++) {
                 image.set(p,z,r,c, predict(image,z,p,r,c,0));    // normal method: use predict() for interpolation
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
          for (uint32_t r = (R>=0 ? R : 0); r < images[0].rows(z); r++) {
            for (Image& image : images) {
              if (image.palette == false) {
               for (uint32_t c = 1; c < image.cols(z); c += 2) {
                image.set(p,z,r,c, predict(image,z,p,r,c,0));
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

    // undo palette before doing the rest of interpolation
    if (images[0].palette && scale == 1) {
      while(images[0].palette && transforms.size()>0) { transforms.back()->invData(images); transforms.pop_back(); ranges = ranges->previous();}
      zoomlevels[0] = zoomlevels[1];
      zoomlevels[2] = zoomlevels[1];
      zoomlevels[3] = zoomlevels[1];
    }

    // interpolate the next zoomlevels
    for (int p = 0; p < ranges->numPlanes() ; ) {
      int z = zoomlevels[p];
      if (z < endZL) { p++; continue; }
      else zoomlevels[p]--;
      if (ranges->min(p) >= ranges->max(p)) continue;
      if ( p == 4 ) continue; // don't interpolate FRA lookback channel
      if ( 1<<(z/2) < scale) continue;
      v_printf_tty(2,"\rINTERPOLATE[%i,%ux%u]                 ",p,images[0].cols(z),images[0].rows(z));
      v_printf_tty(5,"\n");

      if (z % 2 == 0) {
        // horizontal: scan the odd rows
          for (uint32_t r = 1; r < images[0].rows(z); r += 2) {
            for (Image& image : images) {
               for (uint32_t c = 0; c < image.cols(z); c++) {
                 image.set(p,z,r,c, predict(image,z,p,r,c,0));
               }
            }
          }
      } else {
        // vertical: scan the odd columns
          for (uint32_t r = 0; r < images[0].rows(z); r++) {
            for (Image& image : images) {
               for (uint32_t c = 1; c < image.cols(z); c += 2) {
                image.set(p,z,r,c, predict(image,z,p,r,c,0));
               }
            }
          }
      }
    }
    v_printf_tty(2,"\n");
}


// specialized decode functions (for speed)
// assumption: plane and alpha are prepare_zoomlevel(z)
template<typename Coder, typename plane_t, typename alpha_t, int p, typename ranges_t>
void flif_decode_plane_zoomlevel_horizontal(plane_t &plane, Coder &coder, Images &images, const ranges_t *ranges, const alpha_t &alpha, const alpha_t &planeY, Properties &properties,
    const int z, const int fr, const uint32_t r,  const bool alphazero, const bool FRA, const int predictor, const int invisible_predictor) {
    ColorVal min,max;
    Image& image = images[fr];
    if (image.seen_before >= 0) {
        const uint32_t cs = image.zoom_colpixelsize(z)>>image.getscale(), rs = image.zoom_rowpixelsize(z)>>image.getscale();
        copy_row_range(plane,images[image.seen_before].getPlane(p),rs*r,0,cs*image.cols(z),cs); return;
    }
    uint32_t begin=0, end=image.cols(z);
    if (fr>0) {
        begin=image.col_begin[r*image.zoom_rowpixelsize(z)]/image.zoom_colpixelsize(z);
        end=1+(image.col_end[r*image.zoom_rowpixelsize(z)]-1)/image.zoom_colpixelsize(z);
        if (alphazero && p < 3) {
            for (uint32_t c = 0; c < begin; c++)
                if (alpha.get(z,r,c) == 0) plane.set(z,r,c, predict_plane_horizontal(plane,z,p,r,c, image.rows(z), invisible_predictor));
                else image.set(p,z,r,c,images[fr-1](p,z,r,c));
// have to wait with the end of the row until the part between begin and end is decoded
//            for (uint32_t c = end; c < image.cols(z); c++)
//                if (alpha.get(z,r,c) == 0) plane.set(z,r,c, predict_plane_horizontal(plane,z,p,r,c, image.rows(z), invisible_predictor));
//                else image.set(p,z,r,c,images[fr-1](p,z,r,c));
        }
        else if (p != 4) {
            const uint32_t cs = image.zoom_colpixelsize(z)>>image.getscale(), rs = image.zoom_rowpixelsize(z)>>image.getscale();
            copy_row_range(plane, images[fr - 1].getPlane(p), rs*r, cs*0, cs*begin, cs);
            copy_row_range(plane, images[fr - 1].getPlane(p), rs*r, cs*end, cs*image.cols(z), cs);
        }
    } else {
        if (image.numPlanes()>3 && p<3) {begin=0; end=image.cols(z);}
    }
#if LARGE_BINARY > 0
    // avoid branching for border cases
    if (r > 1 && r < image.rows(z)-1 && !FRA && begin == 0 && end > 3) {
      for (uint32_t c = begin; c < 2; c++) {
        if (alphazero && p<3 && alpha.get_fast(r,c) == 0) { plane.set_fast(r,c,predict_plane_horizontal(plane,z,p,r,c, image.rows(z), invisible_predictor)); continue;}
        ColorVal guess = predict_and_calcProps_plane<plane_t,alpha_t,true,false,p,ranges_t>(properties,ranges,image,plane,planeY,z,r,c,min,max, predictor);
        ColorVal curr = coder.read_int(properties, min - guess, max - guess) + guess;
        plane.set_fast(r,c, curr);
      }
      for (uint32_t c = 2; c < end-2; c++) {
        if (alphazero && p<3 && alpha.get_fast(r,c) == 0) { plane.set_fast(r,c,predict_plane_horizontal(plane,z,p,r,c, image.rows(z), invisible_predictor)); continue;}
        ColorVal guess = predict_and_calcProps_plane<plane_t,alpha_t,true,true,p,ranges_t>(properties,ranges,image,plane,planeY,z,r,c,min,max, predictor);
        ColorVal curr = coder.read_int(properties, min - guess, max - guess) + guess;
        plane.set_fast(r,c, curr);
      }
      for (uint32_t c = end-2; c < end; c++) {
        if (alphazero && p<3 && alpha.get_fast(r,c) == 0) { plane.set_fast(r,c,predict_plane_horizontal(plane,z,p,r,c, image.rows(z), invisible_predictor)); continue;}
        ColorVal guess = predict_and_calcProps_plane<plane_t,alpha_t,true,false,p,ranges_t>(properties,ranges,image,plane,planeY,z,r,c,min,max, predictor);
        ColorVal curr = coder.read_int(properties, min - guess, max - guess) + guess;
        plane.set_fast(r,c, curr);
      }
    } else
#endif
    {
      for (uint32_t c = begin; c < end; c++) {
        if (alphazero && p<3 && alpha.get_fast(r,c) == 0) { plane.set_fast(r,c,predict_plane_horizontal(plane,z,p,r,c, image.rows(z), invisible_predictor)); continue;}
        if (FRA && p<4 && image.getFRA(z,r,c) > 0) { plane.set_fast(r,c,images[fr-image.getFRA(z,r,c)](p,z,r,c)); continue;}
        ColorVal guess = predict_and_calcProps_plane<plane_t,alpha_t,true,false,p,ranges_t>(properties,ranges,image,plane,planeY,z,r,c,min,max, predictor);
        if (FRA && p==4 && max > fr) max = fr;
        if (FRA && (guess>max || guess<min)) guess = min;
        ColorVal curr = coder.read_int(properties, min - guess, max - guess) + guess;
        plane.set_fast(r,c, curr);
      }
    }
    if (fr>0 && alphazero && p < 3) {
            for (uint32_t c = end; c < image.cols(z); c++)
                if (alpha.get(z,r,c) == 0) plane.set(z,r,c, predict_plane_horizontal(plane,z,p,r,c, image.rows(z), invisible_predictor));
                else image.set(p,z,r,c,images[fr-1](p,z,r,c));
    }
}

template<typename Coder, typename plane_t, typename alpha_t, int p, typename ranges_t>
void flif_decode_plane_zoomlevel_vertical(plane_t &plane, Coder &coder, Images &images, const ranges_t *ranges, const alpha_t &alpha, const alpha_t &planeY, Properties &properties,
    const int z, const int fr, const uint32_t r,  const bool alphazero, const bool FRA, const int predictor, const int invisible_predictor) {
    ColorVal min,max;
    Image& image = images[fr];
    if (image.seen_before >= 0) {
        const uint32_t cs = image.zoom_colpixelsize(z)>>image.getscale(), rs = image.zoom_rowpixelsize(z)>>image.getscale();
        copy_row_range(plane, images[image.seen_before].getPlane(p),rs*r,cs*1,cs*image.cols(z),cs*2); return;
    }
    uint32_t begin=1, end=image.cols(z);
    if (fr>0) {
        begin=(image.col_begin[r*image.zoom_rowpixelsize(z)]/image.zoom_colpixelsize(z));
        end=(1+(image.col_end[r*image.zoom_rowpixelsize(z)]-1)/image.zoom_colpixelsize(z))|1;
        if (begin>1 && ((begin&1) ==0)) begin--;
        if (begin==0) begin=1;
        if (alphazero && p < 3) {
            for (uint32_t c = 1; c < begin; c += 2)
                if (alpha.get(z, r, c) == 0) plane.set(z, r, c, predict_plane_vertical(plane, z, p, r, c, image.cols(z), invisible_predictor));
                else image.set(p,z,r,c,images[fr-1](p,z,r,c));
// have to wait with the end of the row until the part between begin and end is decoded
//            for (uint32_t c = end; c < image.cols(z); c += 2)
//                if (alpha.get(z, r, c) == 0) plane.set(z, r, c, predict_plane_vertical(plane, z, p, r, c, image.cols(z), invisible_predictor));
//                else image.set(p,z,r,c,images[fr-1](p,z,r,c));
        }
        else if (p != 4) {
            const uint32_t cs = image.zoom_colpixelsize(z)>>image.getscale(), rs = image.zoom_rowpixelsize(z)>>image.getscale();
            copy_row_range(plane, images[fr - 1].getPlane(p), rs*r, cs*1, cs*begin, cs*2);
            copy_row_range(plane, images[fr - 1].getPlane(p), rs*r, cs*end, cs*image.cols(z), cs*2);
        }
    } else {
        if (image.numPlanes()>3 && p<3) {begin=1; end=image.cols(z);}
    }
#if LARGE_BINARY > 0
    // avoid branching for border cases
    if (r > 1 && r < image.rows(z)-1 && !FRA && end == image.cols(z) && end > 5 && begin == 1) {
      uint32_t c = begin;
      for (; c < 3; c+=2) {
        if (alphazero && p<3 && alpha.get_fast(r,c) == 0) { plane.set_fast(r,c,predict_plane_vertical(plane, z, p, r, c, image.cols(z), invisible_predictor)); continue;}
        ColorVal guess = predict_and_calcProps_plane<plane_t,alpha_t,false,false,p,ranges_t>(properties,ranges,image,plane,planeY,z,r,c,min,max, predictor);
        ColorVal curr = coder.read_int(properties, min - guess, max - guess) + guess;
        plane.set_fast(r,c, curr);
      }
      for (; c < end-2; c+=2) {
        if (alphazero && p<3 && alpha.get_fast(r,c) == 0) { plane.set_fast(r,c,predict_plane_vertical(plane, z, p, r, c, image.cols(z), invisible_predictor)); continue;}
        ColorVal guess = predict_and_calcProps_plane<plane_t,alpha_t,false,true,p,ranges_t>(properties,ranges,image,plane,planeY,z,r,c,min,max, predictor);
        ColorVal curr = coder.read_int(properties, min - guess, max - guess) + guess;
        plane.set_fast(r,c, curr);
      }
      for (; c < end; c+=2) {
        if (alphazero && p<3 && alpha.get_fast(r,c) == 0) { plane.set_fast(r,c,predict_plane_vertical(plane, z, p, r, c, image.cols(z), invisible_predictor)); continue;}
        ColorVal guess = predict_and_calcProps_plane<plane_t,alpha_t,false,false,p,ranges_t>(properties,ranges,image,plane,planeY,z,r,c,min,max, predictor);
        ColorVal curr = coder.read_int(properties, min - guess, max - guess) + guess;
        plane.set_fast(r,c, curr);
      }
    } else
#endif
    {
      for (uint32_t c = begin; c < end; c+=2) {
        if (alphazero && p<3 && alpha.get_fast(r,c) == 0) { plane.set_fast(r,c,predict_plane_vertical(plane, z, p, r, c, image.cols(z), invisible_predictor)); continue;}
        if (FRA && p<4 && image.getFRA(z,r,c) > 0) { plane.set_fast(r,c,images[fr-image.getFRA(z,r,c)](p,z,r,c)); continue;}
        ColorVal guess = predict_and_calcProps_plane<plane_t,alpha_t,false,false,p,ranges_t>(properties,ranges,image,plane,planeY,z,r,c,min,max, predictor);
        if (FRA && p==4 && max > fr) max = fr;
        if (FRA && (guess>max || guess<min)) guess = min;
        ColorVal curr = coder.read_int(properties, min - guess, max - guess) + guess;
        //plane.set(z,r,c, curr);
        plane.set_fast(r,c, curr);
      }
    }
    if (fr>0 && alphazero && p < 3) {
            for (uint32_t c = end; c < image.cols(z); c += 2)
                if (alpha.get(z, r, c) == 0) plane.set(z, r, c, predict_plane_vertical(plane, z, p, r, c, image.cols(z), invisible_predictor));
                else image.set(p,z,r,c,images[fr-1](p,z,r,c));
    }
}

//TODO use tuples or something to make this less ugly/more generic
template<typename Coder, typename alpha_t, typename ranges_t>
struct horizontal_plane_decoder: public PlaneVisitor {
    Coder &coder; Images &images; const ranges_t *ranges; Properties &properties; const int z; const bool alphazero, FRA;
    uint32_t r; int fr; const alpha_t *alpha; const alpha_t *planeY; const int predictor; const int invisible_predictor; const int p;

    horizontal_plane_decoder(Coder &c, Images &i, const ranges_t *ra, Properties &prop, const int zl, const bool az, const bool fra, const int pred, const int invisible_pred, const int plane) :
        coder(c), images(i), ranges(ra), properties(prop), z(zl), alphazero(az), FRA(fra), predictor(pred), invisible_predictor(invisible_pred), p(plane) {}

    void prepare_row(uint32_t row, int frame, const alpha_t *a, const alpha_t *pY) {
        r = row;
        fr = frame;
        alpha = a;
        planeY = pY;
    }
    void visit(Plane<ColorVal_intern_8>   &plane) override {
        // this branching on plane number is just to avoid too much template code blowup
        if (p==0) flif_decode_plane_zoomlevel_horizontal<Coder,Plane<ColorVal_intern_8>,alpha_t,0,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
        if (p==3) flif_decode_plane_zoomlevel_horizontal<Coder,Plane<ColorVal_intern_8>,alpha_t,3,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
        if (p==4) flif_decode_plane_zoomlevel_horizontal<Coder,Plane<ColorVal_intern_8>,alpha_t,4,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
    }
    void visit(Plane<ColorVal_intern_16>  &plane) override {
        if (p==1) flif_decode_plane_zoomlevel_horizontal<Coder,Plane<ColorVal_intern_16>,alpha_t,1,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
        if (p==2) flif_decode_plane_zoomlevel_horizontal<Coder,Plane<ColorVal_intern_16>,alpha_t,2,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
    }
#ifdef SUPPORT_HDR
    void visit(Plane<ColorVal_intern_16u> &plane) override {
        if (p==0) flif_decode_plane_zoomlevel_horizontal<Coder,Plane<ColorVal_intern_16u>,alpha_t,0,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
        if (p==3) flif_decode_plane_zoomlevel_horizontal<Coder,Plane<ColorVal_intern_16u>,alpha_t,3,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
    }
    void visit(Plane<ColorVal_intern_32>  &plane) override {
        if (p==1) flif_decode_plane_zoomlevel_horizontal<Coder,Plane<ColorVal_intern_32>,alpha_t,1,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
        if (p==2) flif_decode_plane_zoomlevel_horizontal<Coder,Plane<ColorVal_intern_32>,alpha_t,2,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
    }
#endif
//    void visit(ConstantPlane              &plane) override {flif_decode_plane_zoomlevel_horizontal<Coder,ConstantPlane,alpha_t,p>(plane,coder,images,ranges,*alpha,properties,z,fr,r,alphazero,FRA);}
};

//TODO use tuples or something to make this less ugly/more generic
template<typename Coder, typename alpha_t, typename ranges_t>
struct vertical_plane_decoder: public PlaneVisitor {
    Coder &coder; Images &images; const ranges_t *ranges; Properties &properties; const int z; const bool alphazero, FRA;
    uint32_t r; int fr; const alpha_t *alpha; const alpha_t *planeY; const int predictor; const int invisible_predictor; const int p;

    vertical_plane_decoder(Coder &c, Images &i, const ranges_t *ra, Properties &prop, const int zl, const bool az, const bool fra, const int pred, const int invisible_pred, const int plane) :
        coder(c), images(i), ranges(ra), properties(prop), z(zl), alphazero(az), FRA(fra), predictor(pred), invisible_predictor(invisible_pred), p(plane) {}

    void prepare_row(uint32_t row, int frame, const alpha_t *a, const alpha_t *pY) {
        r = row;
        fr = frame;
        alpha = a;
        planeY = pY;
    }
    void visit(Plane<ColorVal_intern_8>   &plane) override {
        // this branching on plane number is just to avoid too much template code blowup
        if (p==0) flif_decode_plane_zoomlevel_vertical<Coder,Plane<ColorVal_intern_8>,alpha_t,0,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
        if (p==3) flif_decode_plane_zoomlevel_vertical<Coder,Plane<ColorVal_intern_8>,alpha_t,3,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
        if (p==4) flif_decode_plane_zoomlevel_vertical<Coder,Plane<ColorVal_intern_8>,alpha_t,4,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
    }
    void visit(Plane<ColorVal_intern_16>  &plane) override {
        if (p==1) flif_decode_plane_zoomlevel_vertical<Coder,Plane<ColorVal_intern_16>,alpha_t,1,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
        if (p==2) flif_decode_plane_zoomlevel_vertical<Coder,Plane<ColorVal_intern_16>,alpha_t,2,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
    }
#ifdef SUPPORT_HDR
    void visit(Plane<ColorVal_intern_16u> &plane) override {
        if (p==0) flif_decode_plane_zoomlevel_vertical<Coder,Plane<ColorVal_intern_16u>,alpha_t,0,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
        if (p==3) flif_decode_plane_zoomlevel_vertical<Coder,Plane<ColorVal_intern_16u>,alpha_t,3,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
    }
    void visit(Plane<ColorVal_intern_32>  &plane) override {
        if (p==1) flif_decode_plane_zoomlevel_vertical<Coder,Plane<ColorVal_intern_32>,alpha_t,1,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
        if (p==2) flif_decode_plane_zoomlevel_vertical<Coder,Plane<ColorVal_intern_32>,alpha_t,2,ranges_t>(plane,coder,images,ranges,*alpha,*planeY,properties,z,fr,r,alphazero,FRA, predictor, invisible_predictor);
    }
#endif
//    void visit(ConstantPlane              &plane) override {flif_decode_plane_zoomlevel_vertical<Coder,ConstantPlane,alpha_t,p>(plane,coder,images,ranges,*alpha,properties,z,fr,r,alphazero,FRA);}
};

template<typename IO, typename Rac, typename Coder, typename alpha_t, typename ranges_t>
bool flif_decode_FLIF2_inner_horizontal(const int p, IO& io, Rac &rac, std::vector<Coder> &coders, Images &images, const ranges_t *ranges,
                             const int beginZL, const int endZL, int quality, int scale, const int i, const int z, const int predictor, std::vector<int>& zoomlevels, std::vector<Transform<IO>*> &transforms, const int invisible_predictor) {
    const int nump = images[0].numPlanes();
    const bool alphazero = images[0].alpha_zero_special;
    const bool FRA = (nump == 5);
    Properties properties((nump>3?NB_PROPERTIESA[p]:NB_PROPERTIES[p]));
    horizontal_plane_decoder<Coder,alpha_t,ranges_t> rowdecoder(coders[p],images,ranges,properties,z,alphazero,FRA, predictor, invisible_predictor,p);
          for (uint32_t r = 1; r < images[0].rows(z); r += 2) {
            if (images[0].cols() == 0) return false; // decode aborted
            pixels_done += images[0].cols(z);
            if (endZL == 0 && (r & 257)==257) v_printf_tty(3,"\r%i%% done [%i/%i] DEC[%i,%ux%u]  ",(int)(100*pixels_done/pixels_todo),i,plane_zoomlevels(images[0], beginZL, endZL)-1,p,images[0].cols(z),images[0].rows(z));
#ifdef CHECK_FOR_BROKENFILES
            if (io.isEOF()) {
              v_printf(1,"Row %i: Unexpected file end. Interpolation from now on.\n",r);
              flif_decode_FLIF2_inner_interpol(images, ranges, p, beginZL, endZL, (r>1?r-2:r), scale, zoomlevels, transforms);
              return false;
            }
#endif
            for (int fr=0; fr<(int)images.size(); fr++) {
                Image &image = images[fr];
                GeneralPlane &plane = image.getPlane(p);
                //ConstantPlane null_alpha(1);
                const alpha_t &planeY = static_cast<const alpha_t&>(image.getPlane(0));
                const alpha_t &alpha = nump > 3 ? static_cast<const alpha_t&>(image.getPlane(3)) : planeY; //null_alpha;
                rowdecoder.prepare_row(r,fr, &alpha, &planeY);
                plane.accept_visitor(rowdecoder);
            }
          }
          return true;
}
template<typename IO, typename Rac, typename Coder, typename alpha_t, typename ranges_t>
bool flif_decode_FLIF2_inner_vertical(const int p, IO& io, Rac &rac, std::vector<Coder> &coders, Images &images, const ranges_t *ranges,
                             const int beginZL, const int endZL, int quality, int scale, const int i, const int z, const int predictor, std::vector<int>& zoomlevels, std::vector<Transform<IO>*> &transforms, const int invisible_predictor) {
    const int nump = images[0].numPlanes();
    const bool alphazero = images[0].alpha_zero_special;
    const bool FRA = (nump == 5);
    Properties properties((nump>3?NB_PROPERTIESA[p]:NB_PROPERTIES[p]));
    vertical_plane_decoder<Coder,alpha_t,ranges_t> rowdecoder(coders[p],images,ranges,properties,z,alphazero,FRA, predictor, invisible_predictor,p);
          for (uint32_t r = 0; r < images[0].rows(z); r++) {
            if (images[0].cols() == 0) return false; // decode aborted
            pixels_done += images[0].cols(z)/2;
            if (endZL == 0 && (r&513)==513) v_printf_tty(3,"\r%i%% done [%i/%i] DEC[%i,%ux%u]  ",(int)(100*pixels_done/pixels_todo),i,plane_zoomlevels(images[0], beginZL, endZL)-1,p,images[0].cols(z),images[0].rows(z));
#ifdef CHECK_FOR_BROKENFILES
            if (io.isEOF()) {
              v_printf(1,"Row %i: Unexpected file end. Interpolation from now on.\n", r);
              flif_decode_FLIF2_inner_interpol(images, ranges, p, beginZL, endZL, (r>0?r-1:r), scale, zoomlevels, transforms);
              return false;
            }
#endif
            for (int fr=0; fr<(int)images.size(); fr++) {
                Image &image = images[fr];
                GeneralPlane &plane = image.getPlane(p);
                //ConstantPlane null_alpha(1);
                const alpha_t &planeY = static_cast<const alpha_t&>(image.getPlane(0));
                const alpha_t &alpha = nump > 3 ? static_cast<const alpha_t&>(image.getPlane(3)) : planeY; //null_alpha;
                rowdecoder.prepare_row(r,fr, &alpha, &planeY);
                plane.accept_visitor(rowdecoder);
            }
          }

          return true;
}


template<typename IO, typename Rac, typename Coder, typename ranges_t>
bool flif_decode_FLIF2_inner(IO& io, Rac &rac, std::vector<Coder> &coders, Images &images, const ranges_t *ranges,
                             const int beginZL, const int endZL, int quality, int scale, std::vector<Transform<IO>*> &transforms,
                             uint32_t (*callback)(int32_t,int64_t), Images &partial_images, const int invisible_predictor) {
    const int nump = images[0].numPlanes();
//    const bool alphazero = images[0].alpha_zero_special;
//    const bool FRA = (nump == 5);
    // flif_decode
    UniformSymbolCoder<Rac> metaCoder(rac);
    std::vector<int> zoomlevels(nump, beginZL);
    const bool default_order = metaCoder.read_int(0, 1);
    int the_predictor[5] = {0,0,0,0,0};
    for (int p=0; p<nump; p++) the_predictor[p] = metaCoder.read_int(-1, MAX_PREDICTOR);
    for (int i = 0; i < plane_zoomlevels(images[0], beginZL, endZL); i++) {
      int p;
      if (default_order) {
        std::pair<int, int> pzl = plane_zoomlevel(images[0], beginZL, endZL, i, ranges);
        p = pzl.first;
        assert(zoomlevels[p] == pzl.second);
      } else {
        p = metaCoder.read_int(0, nump-1);
      }
      int z = zoomlevels[p];
      if (z < 0) {e_printf("Corrupt file: invalid plane/zoomlevel\n"); return false;}
      if ((100*pixels_done > quality*pixels_todo)) {
              v_printf(5,"%lu subpixels done, %lu subpixels todo, quality target %i%% reached (%i%%)\n",(long unsigned)pixels_done,(long unsigned)pixels_todo,(int)quality,(int)(100*pixels_done/pixels_todo));
              flif_decode_FLIF2_inner_interpol(images, ranges, p, beginZL, endZL, -1, scale, zoomlevels, transforms);
              return false;
      }
      if (ranges->min(p) < ranges->max(p)) {
        int predictor;
        if (the_predictor[p]<0) predictor = metaCoder.read_int(0, MAX_PREDICTOR);
        else predictor = the_predictor[p];
        if (1<<(z/2) < scale) {
              v_printf(5,"%lu subpixels done (out of %lu subpixels at this scale), scale target 1:%i reached\n",(long unsigned)pixels_done,(long unsigned)pixels_todo,(int)scale);
              flif_decode_FLIF2_inner_interpol(images, ranges, p, beginZL, endZL, -1, scale, zoomlevels, transforms);
              return false;
        }
        if (endZL == 0) v_printf_tty(2,"\r%i%% done [%i/%i] DEC[%i,%ux%u]  ",(int)(100*pixels_done/pixels_todo),i,plane_zoomlevels(images[0], beginZL, endZL)-1,p,images[0].cols(z),images[0].rows(z));
        for (Image& image : images) { image.getPlane(p).prepare_zoomlevel(z); }
        if (p>0) for (Image& image : images) { image.getPlane(0).prepare_zoomlevel(z); }
        if (p<3 && nump>3) for (Image& image : images) { image.getPlane(3).prepare_zoomlevel(z); }

//        ConstantPlane null_alpha(1);
//        GeneralPlane &alpha = nump > 3 ? images[0].getPlane(3) : null_alpha;
        if (z % 2 == 0) {
                if (images[0].getDepth() <= 8) { if (!flif_decode_FLIF2_inner_horizontal<IO,Rac,Coder,Plane<ColorVal_intern_8>,ranges_t>(p,io, rac, coders, images, ranges, beginZL, endZL, quality, scale, i, z, predictor, zoomlevels, transforms, invisible_predictor)) return false;}
#ifdef SUPPORT_HDR
                else if (images[0].getDepth() > 8) { if (!flif_decode_FLIF2_inner_horizontal<IO,Rac,Coder,Plane<ColorVal_intern_16u>,ranges_t>(p,io, rac, coders, images, ranges, beginZL, endZL, quality, scale, i, z, predictor, zoomlevels, transforms, invisible_predictor)) return false; }
#endif
        } else {
                if (images[0].getDepth() <= 8) { if (!flif_decode_FLIF2_inner_vertical<IO,Rac,Coder,Plane<ColorVal_intern_8>,ranges_t>(p,io, rac, coders, images, ranges, beginZL, endZL, quality, scale, i, z, predictor, zoomlevels, transforms, invisible_predictor)) return false;}
#ifdef SUPPORT_HDR
                else if (images[0].getDepth() > 8) { if (!flif_decode_FLIF2_inner_vertical<IO,Rac,Coder,Plane<ColorVal_intern_16u>,ranges_t>(p,io, rac, coders, images, ranges, beginZL, endZL, quality, scale, i, z, predictor, zoomlevels, transforms, invisible_predictor)) return false;}
#endif

        }
        if (endZL==0) {
          v_printf(3,"    read %li bytes   ", io.ftell());
          v_printf(5,"\n");
        }
        zoomlevels[p]--;
        int qual = 10000*pixels_done/pixels_todo;
        if (callback && p<4 && (endZL==0 || i+1 == plane_zoomlevels(images[0], beginZL, endZL)) && qual >= progressive_qual_target) {
          for (unsigned int n=0; n < images.size(); n++) partial_images[n] = images[n].clone(); // make a copy to work with
          int64_t pixels_really_done = pixels_done;
          std::vector<Transform<IO>*> transforms_copy = transforms;
          std::vector<int> zoomlevels_copy = zoomlevels;
          flif_decode_FLIF2_inner_interpol(partial_images, ranges, 0, beginZL, endZL, -1, scale, zoomlevels_copy, transforms_copy);
          if (endZL>0) flif_decode_FLIF2_inner_interpol(partial_images, ranges, 0, endZL-1, 0, -1, scale, zoomlevels_copy, transforms_copy);
          pixels_done = pixels_really_done;
          for (Image& image : partial_images) image.normalize_scale();
          for (int i=transforms_copy.size()-1; i>=0; i--) if (transforms_copy[i]->undo_redo_during_decode()) transforms_copy[i]->invData(partial_images);
          progressive_qual_shown = qual;
          progressive_qual_target = callback(qual,io.ftell());
          if (qual >= progressive_qual_target) return false;
        }
      } else zoomlevels[p]--;
    }
    return true;
}

template<typename IO, typename Rac, typename Coder>
bool flif_decode_FLIF2_pass(IO &io, Rac &rac, Images &images, const ColorRanges *ranges, std::vector<Tree> &forest,
                            const int beginZL, const int endZL, int quality, int scale, std::vector<Transform<IO>*> &transforms,
                            uint32_t (*callback)(int32_t,int64_t), Images &partial_images, int cutoff, int alpha, const int invisible_predictor) {
    std::vector<Coder> coders;
    coders.reserve(images[0].numPlanes());
    for (int p = 0; p < images[0].numPlanes(); p++) {
        Ranges propRanges;
        initPropRanges(propRanges, *ranges, p);
        coders.emplace_back(rac, propRanges, forest[p], 0, cutoff, alpha);
    }

    if (beginZL == images[0].zooms() && endZL > 0) {
      // special case: very left top pixel must be read first to get it all started
      // SimpleSymbolCoder<FLIFBitChanceMeta, Rac, 24> metaCoder(rac);
      UniformSymbolCoder<Rac> metaCoder(rac);
      for (int p = 0; p < images[0].numPlanes(); p++) {
        if (ranges->min(p) < ranges->max(p)) {
          for (Image& image : images) image.set(p,0,0,0, metaCoder.read_int(ranges->min(p), ranges->max(p)));
          pixels_done++;
        }
      }
    }
#if LARGE_BINARY > 1
    // de-virtualize some of those ColorRanges
    if (const ColorRangesCB * rangesCB = dynamic_cast<const ColorRangesCB*>(ranges))
        return flif_decode_FLIF2_inner<IO,Rac,Coder,ColorRangesCB>(io, rac, coders, images, rangesCB, beginZL, endZL, quality, scale, transforms, callback, partial_images, invisible_predictor);
    if (const ColorRangesBounds * rangesB = dynamic_cast<const ColorRangesBounds*>(ranges))
        return flif_decode_FLIF2_inner<IO,Rac,Coder,ColorRangesBounds>(io, rac, coders, images, rangesB, beginZL, endZL, quality, scale, transforms, callback, partial_images, invisible_predictor);
    else
#endif
        return flif_decode_FLIF2_inner<IO,Rac,Coder,ColorRanges>(io, rac, coders, images, ranges, beginZL, endZL, quality, scale, transforms, callback, partial_images, invisible_predictor);
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
        std::vector<Transform<IO>*> &transforms, int quality, int scale, uint32_t (*callback)(int32_t,int64_t), Images &partial_images, int encoding, int cutoff, int alpha, const int invisible_predictor) {

    std::vector<Tree> forest(ranges->numPlanes(), Tree());
    int roughZL = 0;
    if (encoding == 2) {
//      roughZL = images[0].zooms() - NB_NOLEARN_ZOOMS-1;
//      if (roughZL < 0) roughZL = 0;
      UniformSymbolCoder<RacIn<IO>> metaCoder(rac);
      roughZL = metaCoder.read_int(0,images[0].zooms());
//      v_printf(2,"Decoding rough data\n");
      if (!flif_decode_FLIF2_pass<IO, RacIn<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn<IO>, bits> >(io, rac, images, ranges, forest, images[0].zooms(), roughZL+1, 100, scale, transforms, callback, partial_images, cutoff, alpha, invisible_predictor)) {
        std::vector<int> zoomlevels(ranges->numPlanes(),roughZL);
        flif_decode_FLIF2_inner_interpol(images, ranges, 0, roughZL, 0, -1, scale, zoomlevels, transforms);
        return false;
      }
    }
    if (encoding == 2 && (quality <= 0 || pixels_done >= pixels_todo) && pixels_todo > 1) {
      v_printf(3,"Not decoding MANIAC tree (%i pixels done, had %i pixels to do)\n", pixels_done, pixels_todo);
      std::vector<int> zoomlevels(ranges->numPlanes(),roughZL);
      flif_decode_FLIF2_inner_interpol(images, ranges, 0, roughZL, 0, -1, scale, zoomlevels, transforms);
      return pixels_done >= pixels_todo;
    } else {
      v_printf(3,"Decoded header + rough data. Decoding MANIAC tree.\n");
      if (!flif_decode_tree<IO, FLIFBitChanceTree, RacIn<IO>>(io, rac, ranges, forest, encoding)) {
         if (encoding == 2) {
            v_printf(1,"File probably truncated in the middle of MANIAC tree representation. Interpolating.\n");
            std::vector<int> zoomlevels(ranges->numPlanes(),roughZL);
            flif_decode_FLIF2_inner_interpol(images, ranges, 0, roughZL, 0, -1, scale, zoomlevels, transforms);
         }
         return false;
      }
    }


    switch(encoding) {
        case 1: v_printf(3,"Decoding data (scanlines)\n");
                return flif_decode_scanlines_pass<IO, RacIn<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn<IO>, bits> >(io, rac, images, ranges, forest, quality, transforms, callback, partial_images, cutoff, alpha);
                break;
        case 2: v_printf(3,"Decoding data (interlaced)\n");
                return flif_decode_FLIF2_pass<IO, RacIn<IO>, FinalPropertySymbolCoder<FLIFBitChancePass2, RacIn<IO>, bits> >(io, rac, images, ranges, forest, roughZL, 0, quality, scale, transforms, callback, partial_images, cutoff, alpha, invisible_predictor);
                break;
    }
    return false;
}

template <typename IO>
size_t read_big_endian_varint(IO& io) {
    int number;
    size_t result = 0;
    int bytes_read = 0;
    while (bytes_read++ < 10) {
      number = io.getc();
      if (number < 0) break;
      if (number < 128) return result+number;
      number -= 128;
      result += number;
      result <<= 7;
    }
    e_printf("Invalid number encountered!\n");
    return 0;
}

template <typename IO>
int read_chunk(IO& io, MetaData& metadata) {
    metadata.name[0] = io.getc();
//    printf("chunk: %s\n", metadata.name);
    if (metadata.name[0] < 32) {
      if (metadata.name[0] > 0) e_printf("This is not a FLIF16 image, but a more recent FLIF file. Please update your FLIF decoder.\n");
      return 1; // final chunk, stop reading!
    }
    io.gets(metadata.name+1,4);
    if (strcmp(metadata.name,"iCCP")
     && strcmp(metadata.name,"eXif")
     && strcmp(metadata.name,"eXmp")
    ) {
        e_printf("Unknown chunk: %s\n",metadata.name);
        return -1;
    }
    metadata.length = read_big_endian_varint(io);
//    printf("chunk length: %lu\n", metadata.length);
    metadata.contents.resize(metadata.length);
    for(size_t i = 0; i < metadata.length; i++) {
        metadata.contents[i] = io.getc();
    }
    return 0; // read next chunk
}


template <typename IO>
bool flif_decode(IO& io, Images &images, int quality, int scale, uint32_t (*callback)(int32_t,int64_t), int first_callback_quality, Images &partial_images, int rw, int rh, int crc_check, bool fit, metadata_options &md, FLIF_INFO* info) {
    bool just_identify = false;
    bool just_metadata = false;
    if (scale == -1) just_identify=true;
    else if (scale == -2) just_metadata=true;
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
    std::vector<MetaData> metadata;
    int c;


    if (strcmp(buff,"FLIF")) { e_printf("%s is not a FLIF file\n",io.getName()); return false; }


    if (!ioget_int_8bit (io, &c))
        return false;
    if (c < ' ' || c > ' '+32+15+32) { e_printf("Invalid or unknown FLIF format byte\n"); return false;}
    c -= ' ';
    int numFrames=1;
    if (c > 47) {
        c -= 32;
        numFrames = 2; // animation
    }
    const int encoding=c/16;
    if (encoding < 1 || encoding > 2) { e_printf("Invalid or unknown FLIF encoding method\n"); return false;}
    if (scale != 1 && encoding==1 && !just_identify) { e_printf("Cannot decode non-interlaced FLIF file at lower scale!\n"); return false; }
    if (quality < 100 && encoding==1) { v_printf(1,"Cannot decode non-interlaced FLIF file at lower quality! Ignoring quality...\n");}
    int numPlanes=c%16;
    if (numPlanes < 1 || numPlanes > 4 || numPlanes == 2) {e_printf("Invalid FLIF header (unsupported color channels)\n"); return false;}
    if (!ioget_int_8bit (io, &c))
        return false;
    if (c < '0' || c > '2')  {e_printf("Invalid FLIF header (unsupported color depth)\n"); return false;}

    int width = read_big_endian_varint(io) + 1;
    int height = read_big_endian_varint(io) + 1;
    if (width < 1 || height < 1) {e_printf("Invalid FLIF header\n"); return false;}

    if (numFrames > 1) numFrames = read_big_endian_varint(io)+2;

    MetaData chunk;
    int result = 0;
    while (!(result = read_chunk(io, chunk))) {
        if (!md.icc && !strcmp(chunk.name, "iCCP")) continue;
        if (!md.exif && !strcmp(chunk.name, "eXif")) continue;
        if (!md.xmp && !strcmp(chunk.name, "eXmp")) continue;
        v_printf(3,"Read metadata chunk: %s\n",chunk.name);
        metadata.push_back(chunk);
    }
    if (result != 1) {
        e_printf("Invalid FLIF file.\n"); return false;
    }

    if (just_metadata) {
        images.push_back(Image());
        images[0].metadata = metadata;
        return true;
    }


    RacIn<IO> rac(io);
//    SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18> metaCoder(rac);
    UniformSymbolCoder<RacIn<IO>> metaCoder(rac);

//    if (metaCoder.read_int(0,1)) {
//        e_printf("This is not a FLIF16 image, but a more recent FLIF file. Please update your FLIF decoder.\n");
//        return false;
//    }

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
    else if (c=='2') v_printf(3," %i, depth: 16 bit",numPlanes);
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
        if (metadata.size() > 0) {
            v_printf(1, "Contains metadata: ");
            for (size_t i=0; i<metadata.size(); i++) {
                if (!strcmp(metadata[i].name,"iCCP")) v_printf(1, "ICC color profile");
                else if (!strcmp(metadata[i].name,"eXif")) v_printf(1, "Exif");
                else if (!strcmp(metadata[i].name,"eXmp")) v_printf(1, "XMP");
                else v_printf(1, "Unknown: %s", metadata[i].name);
                if (i+1 < metadata.size()) v_printf(1, ", ");
            }
        }
        return true;
    }
    if (info != 0) {
        info->width = width;
        info->height = height;
        info->channels = numPlanes;
        if (c=='1')
            info->bit_depth = 8;
        else if (c=='2')
            info->bit_depth = 16;
        else if (c=='0')
            info->bit_depth = ilog2(maxmax+1);
        info->num_images = numFrames;
        return true; // info was requested, skip the rest of the file
    }
    if (numFrames>1) {
        // ignored for now (assuming loop forever)
        metaCoder.read_int(0, 100); // repeats (0=infinite)
    }
    if (rw < 0 || rh < 0) { e_printf("Negative target dimension? Really?\n"); return false; }
    int target_w = rw, target_h = rh;
    if (fit) {
        if (rw <= 0 && rh <= 0) { e_printf("Invalid target dimensions.\n"); return false;}
        // use larger decode dimensions to make sure we have good chroma
        rw = rw*2-1; rh = rh*2-1;
        // don't upscale
        if (target_w > width) target_w = width;
        if (target_h > height) target_h = height;
    }
    if (rw || rh) {
      if (scale > 1) e_printf("Don't use -s and (-r or -f) at the same time! Ignoring -s...\n");
      scale = 1;
      while ( (rw>0 && (((width-1)/scale)+1) > rw)   || (rh>0 && (((height-1)/scale)+1) > rh) ) scale *= 2;
    }
    if (scale != 1 && encoding==1) { v_printf(1,"Cannot decode non-interlaced FLIF file at lower scale! Ignoring resize target...\n"); scale = 1;}

    int scale_shift = ilog2(scale);
    if (scale_shift>0) v_printf(3,"Decoding downscaled image at scale 1:%i (%ix%i -> %ix%i)\n", scale, width, height, ((width-1)/scale)+1, ((height-1)/scale)+1);
    int bytesperpixel = (maxmax > 255 ? 2 : 1) * (numPlanes + (numPlanes > 1 ? 2 : 0));
    uint64_t estimated_buffer_size = (((width-1)/scale)+1) * (((height-1)/scale)+1) * numFrames * numPlanes * bytesperpixel;
    if (estimated_buffer_size > MAX_IMAGE_BUFFER_SIZE) {
        e_printf("This is going to take too much memory. Aborting.\nCompile with a higher MAX_IMAGE_BUFFER_SIZE if you really want to do this.\n"); return false;
    }

    for (int i=0; i<numFrames; i++) {
      images.push_back(Image(scale_shift));
      if (!images[i].init(width,height,0,maxmax,numPlanes)) return false;
      images[i].alpha_zero_special = alphazero;
      images[i].metadata = metadata;
      if (numFrames>1) images[i].frame_delay = metaCoder.read_int(0, 60000); // time in ms between frames
      if (callback) partial_images.push_back(Image(scale_shift));
      //if (numFrames>1) partial_images[i].frame_delay = images[i].frame_delay;
    }

    int cutoff = 2;
    int alpha = 0xFFFFFFFF / 19;
    if (metaCoder.read_int(0,1)) {
      cutoff = metaCoder.read_int(1,128);
      alpha = 0xFFFFFFFF / metaCoder.read_int(2,128);
      if (metaCoder.read_int(0,1)) {
        e_printf("Not yet implemented: non-default bitchance initialization\n");
        return false;
      }
    }
    std::vector<std::unique_ptr<const ColorRanges>> rangesList;
    std::vector<std::unique_ptr<Transform<IO>>> transforms;
    rangesList.push_back(std::unique_ptr<const ColorRanges>(getRanges(images[0])));
    v_printf(4,"Transforms: ");
    int tcount=0;
    int tnb=0, tpnb=-1;
    while (rac.read_bit()) {
        std::string desc = read_name(rac,tnb);
        if (tnb <= tpnb) {
            e_printf("\nTransformation '%s' is invalid given the previous transformations.\nCorrupt file? Or try upgrading your FLIF decoder?\n", desc.c_str());
            return false;
        }
        tpnb = tnb;
        auto trans = create_transform<IO>(desc);
        auto previous_range = rangesList.back().get();
        if (!trans) {
            e_printf("\nUnknown transformation '%s'\nTry upgrading your FLIF decoder?\n", desc.c_str());
            return false;
        }
        if (!trans->init(previous_range)) {
            e_printf("Transformation '%s' failed\nTry upgrading your FLIF decoder?\n", desc.c_str());
            return false;
        }
        if (tcount++ > 0) v_printf(4,", ");
        v_printf(4,"%s", desc.c_str());
        if (desc == "Frame_Lookback") {
                if (images.size()<2) return false;
                trans->configure(images.size());
        }
        if (desc == "Frame_Shape") {
                if (images.size()<2) return false;
                int unique_frames=images.size()-1; // not considering first frame
                for (Image& i : images) if (i.seen_before >= 0) unique_frames--;
                if (unique_frames < 1) {return false;}
                trans->configure(unique_frames*images[0].rows()); trans->configure(images[0].cols()); }
        if (desc == "Duplicate_Frame") { if (images.size()<2) return false; else trans->configure(images.size()); }
        if (desc == "Palette_Alpha") { trans->configure(images[0].alpha_zero_special); }
        if (!trans->load(previous_range, rac)) return false;
        rangesList.push_back(std::unique_ptr<const ColorRanges>(trans->meta(images, previous_range)));
        if (!rangesList.back().get()) return false;
        transforms.push_back(std::move(trans));
    }

    std::vector<Transform<IO>*> transform_ptrs;
    for(const auto& ptr : transforms)
        transform_ptrs.push_back(ptr.get());

    if (tcount==0) v_printf(4,"none\n"); else v_printf(4,"\n");
    const ColorRanges* ranges = rangesList.back().get();

    int invisible_predictor = 0;
    if (alphazero && ranges->numPlanes() > 3 && ranges->min(3) <= 0 && encoding == 2) {
      invisible_predictor = metaCoder.read_int(0,MAX_PREDICTOR);
      v_printf(4,"Invisible pixel predictor: -H%i\n",invisible_predictor);
    }

    int realnumplanes = 0;
    for (int i=0; i<ranges->numPlanes(); i++) if (ranges->min(i)<ranges->max(i)) realnumplanes++;
    pixels_todo = (int64_t)width*height*realnumplanes/scale/scale;
    pixels_done = 0;
    if (pixels_todo == 0) pixels_todo = pixels_done = 1;
    progressive_qual_target = first_callback_quality;
    progressive_qual_shown = -1;
    v_printf(9,"%lu subpixels done, %lu subpixels todo, quality target %i%%\n",(long unsigned)pixels_done,(long unsigned)pixels_todo,(int)quality);

    for (int p = 0; p < ranges->numPlanes(); p++) {
      v_printf(10,"Plane %i: %i..%i\n",p,ranges->min(p),ranges->max(p));
    }

    for (int p = 0; p < ranges->numPlanes(); p++) {
        if (ranges->min(p) >= ranges->max(p)) {
            v_printf(6,"Constant plane %i at color value %i\n",p,ranges->min(p));
            for (int fr = 0; fr < numFrames; fr++)
                images[fr].make_constant_plane(p,ranges->min(p));
        }
    }
    // Y plane shouldn't be constant, even if it is (because we want to avoid special-casing fast Y plane access)
    for (int fr = 0; fr < numFrames; fr++) images[fr].undo_make_constant_plane(0);

    // Alpha plane is never special if it is never zero
    if (ranges->numPlanes()>3 && ranges->min(3) > 0)
      for (int fr = 0; fr < numFrames; fr++) images[fr].alpha_zero_special = false;

    // Alpha plane is never special if it is always zero
    // (on correct input, all other planes should be constant if alpha is always zero and alpha_zero_special holds, so it doesn't really matter.
    //  on malicious input, we have to check this)
    if (ranges->numPlanes()>3 && ranges->max(3) == 0)
      for (int fr = 0; fr < numFrames; fr++) images[fr].alpha_zero_special = false;

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
    bool fully_decoded;
    if (bits == 10) {
       fully_decoded = flif_decode_main<10>(rac, io, images, ranges, transform_ptrs, quality, scale, callback, partial_images, encoding, cutoff, alpha, invisible_predictor);
#ifdef SUPPORT_HDR
    } else {
       fully_decoded = flif_decode_main<18>(rac, io, images, ranges, transform_ptrs, quality, scale, callback, partial_images, encoding, cutoff, alpha, invisible_predictor);
#endif
    }

   v_printf_tty(2,"\r");
   if (numFrames==1)
      v_printf(2,"Decoded input file %s, %li bytes for %ux%u pixels (%.4fbpp)   \n",io.getName(),io.ftell(), images[0].cols()/scale, images[0].rows()/scale, 8.0*io.ftell()/images[0].rows()*scale*scale/images[0].cols());
    else
      v_printf(2,"Decoded input file %s, %li bytes for %i frames of %ux%u pixels (%.4fbpp)   \n",io.getName(),io.ftell(), numFrames, images[0].cols()/scale, images[0].rows()/scale, 8.0*io.ftell()/numFrames/images[0].rows()*scale*scale/images[0].cols());

    bool contains_checksum = metaCoder.read_int(0,1);

    for (Image& i : images) {
        i.normalize_scale();
        if (fully_decoded && quality>=100 && scale==1) // && contains_checksum)
            i.fully_decoded=true;
    }

    while(!transform_ptrs.empty()) {
        transform_ptrs.back()->invData(images);
        transform_ptrs.pop_back();
    }
    transforms.clear();
    rangesList.clear();

    // don't bother making the invisible pixels black if we're not checking the crc anyway
    if (alphazero && crc_check) for (Image& image : images) image.make_invisible_rgb_black();

    if (!crc_check) {
      v_printf(3,"Not checking checksum, as requested.\n");
    } else if (quality>=100 && scale==1 && fully_decoded) {
      if (contains_checksum) {
        const uint32_t checksum = images[0].checksum();
        v_printf(8,"Computed checksum: %X\n", checksum);
        uint32_t checksum2 = metaCoder.read_int(16);
        checksum2 *= 0x10000;
        checksum2 += metaCoder.read_int(16);
        v_printf(8,"Read checksum: %X\n", checksum2);
        if (checksum != checksum2) {
          v_printf(1,"\nCORRUPTION DETECTED: checksums don't match (computed: %x v/s read: %x)! (partial file?)\n\n", checksum, checksum2);
        } else {
          v_printf(3,"Checksum verified.\n");
        }
      } else {
        v_printf(3,"Image does not contain a checksum.\n");
      }
    } else if (quality < 100 || scale > 1) {
      v_printf(3,"Not checking checksum, lossy partial decoding was chosen.\n");
    } else {
      v_printf(1,"File ended prematurely or decoding was interrupted.\n");
    }


    // downscale to target_w, target_h
    if (fit) {
        if (target_w <= 0) target_w = target_h*width/height;
        if (target_h <= 0) target_h = target_w*height/width;
        if (target_w != (int)images[0].cols() || target_h != (int)images[0].rows()) {
          v_printf(3,"Downscaling to %ix%i\n",target_w,target_h);
          for (unsigned int n=0; n < images.size(); n++) images[n] = Image(images[n],target_w,target_h);
        }
    }

    // ensure that the callback gets called even if the image is completely constant
    if (progressive_qual_target > 10000) progressive_qual_target = 10000;
    if (callback && progressive_qual_target > progressive_qual_shown) {
        for (unsigned int n=0; n < images.size(); n++) partial_images[n] = images[n].clone(); // make a copy to work with
        callback(10000*pixels_done/pixels_todo,io.ftell());
    }

    return true;
}


template bool flif_decode(FileIO& io, Images &images, int quality, int scale, uint32_t (*callback)(int32_t,int64_t), int, Images &partial_images, int, int, int, bool, metadata_options &, FLIF_INFO* info);
template bool flif_decode(BlobReader& io, Images &images, int quality, int scale, uint32_t (*callback)(int32_t,int64_t), int, Images &partial_images, int, int, int, bool, metadata_options &, FLIF_INFO* info);
