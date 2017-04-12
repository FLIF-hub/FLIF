/*
FLIF - Free Lossless Image Format

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

#pragma once

#include <vector>

#include "transform.hpp"
#include "../maniac/symbol.hpp"


#define MAX_PER_BUCKET_0 255
#define MAX_PER_BUCKET_1 510
// if bucket0 and bucket1 are big enough to be exact (and no simplification happens!), then we don't need the bit to check for empty buckets
//#define MAX_PER_BUCKET_1 20

#define MAX_PER_BUCKET_2 5
#define MAX_PER_BUCKET_3 255
const unsigned int max_per_colorbucket[] = {MAX_PER_BUCKET_0, MAX_PER_BUCKET_1, MAX_PER_BUCKET_2, MAX_PER_BUCKET_3};

// quantization constants
#define CB0a 1
#define CB0b 1
#define CB1 4


static int totaldiscretecolors=0;
static int totalcontinuousbuckets=0;

typedef int16_t ColorValCB; // not doing this transform for high-bit-depth images anyway

class ColorBucket {
public:
    std::vector<ColorValCB> snapvalues;
    std::vector<ColorValCB> values;
    ColorValCB min;
    ColorValCB max;
    bool discrete;

    ColorBucket() {
        min = 10000;  // +infinity
        max = -10000; // -infinity    (set to empty interval to start with)
        discrete = true;
    }
    void addColor(const ColorVal c, const unsigned int max_per_bucket) {
        if (c<min) min=c;
        if (c>max) max=c;
        if (discrete) {
          unsigned int pos=0;
          for(; pos < values.size(); pos++) {
                if (c == values[pos]) return;
                if (values[pos] > c) break;
          }
          if (values.size() < max_per_bucket) {
                values.insert(values.begin()+pos, c);
                totaldiscretecolors++;
          } else {
                totaldiscretecolors -= max_per_bucket;
                values.clear();
                discrete=false;
                totalcontinuousbuckets++;
          }
        }
    }

    bool removeColor(const ColorVal c) {
        if (discrete) {
          unsigned int pos=0;
          for(; pos < values.size(); pos++) {
                if (c == values[pos]) {
                   values.erase(values.begin()+pos);
                   break;
                }
          }
          if (values.size() == 0) {
            min = 10000;
            max = -10000;
            return true;
          }
          assert(values.size() > 0);
          if (c==min) min = values[0];
          if (c==max) max = values[values.size()-1];
        } else {
          if (c==min) min++;
          if (c==max) max--;
          if (c>max) return true;
          if (c<min) return true;
          discrete=true;
          values.clear();
          for (ColorVal x=min; x <= max; x++) {
            if (x != c) values.push_back(x);
          }
        }
        return true;
    }

    bool empty() const {
       return (min>max);
    }
    ColorVal snapColor_slow(const ColorVal c) const {
        if (c <= min) return min;
        if (c >= max) return max;
        if (discrete) {
          ColorVal mindiff = abs(c-min);
          unsigned int best = 0;
          for(unsigned int i=1; i < values.size(); i++) {
                if (c == values[i]) return c;
                ColorVal diff = abs(c-values[i]);
                if (diff < mindiff) {best = i; mindiff = diff;}
                if (values[i] > c) break; // can safely skip the rest, values is sorted
          }
          return values[best];
        }
        return c;
    }
    void prepare_snapvalues() {
        if (discrete) {
                snapvalues.clear();
                for (ColorVal c=min; c<max; c++) {
                        snapvalues.push_back(snapColor_slow(c));
                }
        }
    }
    void simplify_lossless() {
        if (discrete) {
                if ((int)values.size() == max-min+1) {
                        discrete=false;  // bucket actually contains a continuous range
                        totaldiscretecolors -= values.size();
                        totalcontinuousbuckets++;
                        values.clear();
                }
        }
    }
    void simplify(int percent) {
        if (empty()) return;
        simplify_lossless();
        if (discrete) {
                // heuristic: turn discrete bucket into a continuous one if it is dense enough
                if ((int)values.size()-2 > (max-min-1)*percent/100) {
                        discrete=false; // more than <percent> of the ]min,max[ values are present
                        totaldiscretecolors -= values.size();
                        totalcontinuousbuckets++;
                        values.clear();
                }
        }
    }

    ColorVal snapColor(const ColorVal c) const {
        if (c <= min) return min;
        if (c >= max) return max;
        if (discrete) {
          assert((ColorVal)snapvalues.size() > c-min);
          return snapvalues[c-min];
        }
        return c;
    }

    void print() const {
        if (min>max) v_printf(10,"E ");
        else if (min==max) v_printf(10,"S%i ",min);
        else {
                if (discrete) {
                        v_printf(10,"D[");
                        for (ColorVal c : values) v_printf(10,"%i%c",c,(c<max?',':']'));
                        v_printf(10," ");
                } else {
                        v_printf(10,"C%i..%i ",min,max);
                }
        }
    }
/*
    void printshort() const {
        if (min>max) printf(".");
        else if (min==max) printf("1");
        else {
                if (discrete) {
                        if (values.size()>9) printf("+"); else printf("%u",(unsigned int) values.size());
                } else {
                        printf("C");
                }
        }
    }
*/
};

class ColorBuckets {
public:
    ColorBucket bucket0;
    int min0, min1;
    std::vector<ColorBucket> bucket1;
    std::vector<std::vector<ColorBucket> > bucket2;
    ColorBucket bucket3;
    ColorBucket empty_bucket;
    const ColorRanges *ranges;
    explicit ColorBuckets(const ColorRanges *r) : bucket0(), min0(r->min(0)), min1(r->min(1)),
                                         bucket1((r->max(0) - min0)/CB0a +1),
                                         bucket2((r->max(0) - min0)/CB0b +1, std::vector<ColorBucket>((r->max(1) - min1)/CB1 +1)),
                                         bucket3(),
                                         ranges(r) {}
    ColorBucket& findBucket(const int p, const prevPlanes &pp) {
        assert(p>=0); assert(p<4);
        if (p==0) return bucket0;
        if (p==1) { assert((pp[0]-min0)/CB0a >= 0 && (pp[0]-min0)/CB0a < (int)bucket1.size());
                    return bucket1[(pp[0]-min0)/CB0a];}
        if (p==2) { assert((pp[0]-min0)/CB0b >= 0 && (pp[0]-min0)/CB0b < (int)bucket2.size());
                    assert((pp[1]-min1)/CB1 >= 0 && (pp[1]-min1)/CB1 < (int) bucket2[(pp[0]-min0)/CB0b].size());
                    return bucket2[(pp[0]-min0)/CB0b][(pp[1]-min1)/CB1]; }
        else return bucket3;
    }
    const ColorBucket& findBucket(const int p, const prevPlanes &pp) const {
        assert(p>=0); assert(p<4);
        if (p==0) return bucket0;
        if (p==1) { int i = (pp[0]-min0)/CB0a;
                    if(i >= 0 && i < (int)bucket1.size()) return bucket1[i];
                    else return empty_bucket;}
                    
        if (p==2) { int i=(pp[0]-min0)/CB0b, j = (pp[1]-min1)/CB1;
                    if(i >= 0 && i < (int)bucket2.size() && j >= 0 && j < (int) bucket2[i].size()) return bucket2[i][j];
                    else return empty_bucket;}
        else return bucket3;
    }
    void addColor(const std::vector<ColorVal> &pixel) {
        for (unsigned int p=0; p < pixel.size(); p++) {
                findBucket(p, pixel).addColor(pixel[p],max_per_colorbucket[p]);
        }
    }

    bool exists(const int p, const prevPlanes &pp) const {
        if (p>0 && (pp[0] < min0 || pp[0] > ranges->max(0))) return false;
        if (p>1 && (pp[1] < min1 || pp[1] > ranges->max(1))) return false;

        ColorVal rmin, rmax;
        ColorVal v=pp[p];
        ranges->snap(p,pp,rmin,rmax,v);
        if (v != pp[p]) return false;   // bucket empty because of original range constraints

        const ColorBucket &b = findBucket(p,pp);
        //if (b.min > b.max) return false;
        if (b.snapColor_slow(pp[p]) != pp[p]) return false;
        return true;
    }
    bool exists(const int p, const prevPlanes &lower, const prevPlanes &upper) const {
        prevPlanes pixel=lower;
        if (p==0) {
           for (pixel[0]=lower[0]; pixel[0] <= upper[0]; pixel[0]++)
                if (exists(p,pixel)) return true;
        }
        if (p==1) {
           for (pixel[0]=lower[0]; pixel[0] <= upper[0]; pixel[0]++) {
             for (pixel[1]=lower[1]; pixel[1] <= upper[1]; pixel[1]++) {
                if (exists(p,pixel)) return true;
             }
           }
        }
        return false;
    }

    void print() {
        v_printf(10,"Y buckets:\n");
        bucket0.print();
        v_printf(10,"\nCo buckets:\n");
        for (auto b : bucket1) b.print();
        v_printf(10,"\nCg buckets:\n  ");
        for (auto bs : bucket2) { for (auto b : bs) b.print(); v_printf(10,"\n  ");}
        if (ranges->numPlanes() > 3) {
          v_printf(10,"Alpha buckets:\n");
          bucket3.print();
        }
    }

};

class ColorRangesCB final : public ColorRanges {
protected:
    const ColorRanges *ranges;
    const ColorBuckets *buckets;
    const ColorBucket& bucket(const int p, const prevPlanes &pp) const { return buckets->findBucket(p,pp); }
public:
    ColorRangesCB(const ColorRanges *rangesIn, ColorBuckets *cbIn) :  ranges(rangesIn), buckets(cbIn) {} //print();}
    ~ColorRangesCB() {
        delete buckets;
    }
    bool isStatic() const override { return false; }
    int numPlanes() const override { return ranges->numPlanes(); }
    ColorVal min(int p) const override { return ranges->min(p); }
    ColorVal max(int p) const override { return ranges->max(p); }
    void snap(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv, ColorVal &v) const override {
        const ColorBucket& b = bucket(p,pp);
        minv=b.min;
        maxv=b.max;
        if (b.min > b.max) { e_printf("Corruption detected!\n"); minv=v=min(p); maxv=max(p); return; } // this should only happen on malicious input files
        v=b.snapColor(v);
    }
    void minmax(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv) const override {
        const ColorBucket& b = bucket(p,pp);
        minv=b.min;
        maxv=b.max;
        if (b.min > b.max) { e_printf("Corruption detected!\n"); minv=min(p); maxv=max(p); } // this should only happen on malicious input files
    }
/*    void print() {
        buckets->print();
    }
*/
};


template <typename IO>
class TransformCB : public Transform<IO> {
public:
    TransformCB()
    : cb(0)
    , really_used(false)
    {
    }
    ~TransformCB() {if (!really_used) delete cb;}
protected:
    ColorBuckets *cb;
    bool really_used;

    bool undo_redo_during_decode() override { return false; }

    const ColorRanges* meta(Images&, const ColorRanges *srcRanges) override {
//        cb->print();

        // in the I buckets, some discrete buckets may have become continuous to keep the colorbucket info small
        // this means some Q buckets are empty, which means that some values from the I buckets can be eliminated
        if (srcRanges->min(2) < srcRanges->max(2)) {  // only do this if the Q buckets are actually in use
          prevPlanes pixelL,pixelU;
          pixelL.push_back(cb->min0);
          pixelU.push_back(cb->min0+CB0b-1);
          pixelL.push_back(cb->min1);
          pixelU.push_back(cb->min1+CB1-1);
          for (auto bv : cb->bucket2) {
                pixelL[1] = cb->min1;
                pixelU[1] = cb->min1+CB1-1;
                for (auto b : bv) {
                        if (b.empty()) {
                                for (ColorVal c=pixelL[1]; c<=pixelU[1]; c++) {
                                  if (!cb->findBucket(1,pixelL).removeColor(c)) return NULL;
                                  if (!cb->findBucket(1,pixelU).removeColor(c)) return NULL;
                                }
                        }
                        pixelL[1] += CB1; pixelU[1] += CB1;
                }
                pixelL[0] += CB0b; pixelU[0] += CB0b;
          }
        }
        cb->bucket0.prepare_snapvalues();
        cb->bucket3.prepare_snapvalues();
        for (auto& b : cb->bucket1) b.prepare_snapvalues();
        for (auto& bv : cb->bucket2) for (auto& b : bv) b.prepare_snapvalues();
//        cb->print();

        really_used = true;
        return new ColorRangesCB(srcRanges, cb);
    }
    bool init(const ColorRanges *srcRanges) override {
        cb = NULL;
        really_used = false;
        if(srcRanges->numPlanes() < 3) return false;
        if (srcRanges->min(1) == srcRanges->max(1) && srcRanges->min(2) == srcRanges->max(2)) return false; // monochrome image
        if (srcRanges->min(0) == 0 && srcRanges->max(0) == 0 && srcRanges->min(2) == 0 && srcRanges->max(2) == 0) return false; // probably palette image
        if (srcRanges->min(0) == srcRanges->max(0) &&
            srcRanges->min(1) == srcRanges->max(1) &&
            srcRanges->min(2) == srcRanges->max(2)) return false; // only alpha plane contains information
//        if (srcRanges->max(0) > 255) return false; // do not attempt this on high bit depth images (TODO: generalize color bucket quantization!)
        if (srcRanges->max(0)-srcRanges->min(0) > 1023) return false; // do not attempt this on high bit depth images (TODO: generalize color bucket quantization!)
        if (srcRanges->max(1)-srcRanges->min(1) > 1023) return false; // max 10 bpc
        if (srcRanges->max(2)-srcRanges->min(2) > 1023) return false;
        if (srcRanges->min(1) == srcRanges->max(1)) return false; // middle channel not used, buckets are not going to help
//        if (srcRanges->min(2) == srcRanges->max(2)) return false;
        cb = new ColorBuckets(srcRanges);
        return true;
    }

    static void minmax(const ColorRanges *srcRanges, const int p, const prevPlanes &lower, const prevPlanes &upper, ColorVal &smin, ColorVal &smax) {
        ColorVal rmin, rmax;
        smin=10000;
        smax=-10000;
        prevPlanes pixel=lower;
        if (p==0) {
                srcRanges->minmax(p,pixel,smin,smax);
        } else if (p==1) {
           for (pixel[0]=lower[0]; pixel[0] <= upper[0]; pixel[0]++) {
                srcRanges->minmax(p,pixel,rmin,rmax);
                if (rmin<smin) smin=rmin;
                if (rmax>smax) smax=rmax;
           }
        } else if (p==2) {
           for (pixel[0]=lower[0]; pixel[0] <= upper[0]; pixel[0]++) {
             for (pixel[1]=lower[1]; pixel[1] <= upper[1]; pixel[1]++) {
                srcRanges->minmax(p,pixel,rmin,rmax);
                if (rmin<smin) smin=rmin;
                if (rmax>smax) smax=rmax;
             }
           }
        } else if (p==3) {
                srcRanges->minmax(p,pixel,smin,smax);
        }
    }

    const ColorBucket load_bucket(std::vector<SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18>> &coder, const ColorRanges *srcRanges, const int plane, const prevPlanes &pixelL, const prevPlanes &pixelU) const {
        ColorBucket b;
        if (plane<3)
        for (int p=0; p<plane; p++) {
                if (!cb->exists(p,pixelL,pixelU)) return b;
        }

        ColorVal smin,smax;
        minmax(srcRanges,plane,pixelL,pixelU,smin,smax);

        int exists = coder[0].read_int2(0, 1);
        if (exists == 0) {
           return b; // empty bucket
        }
        if (smin == smax) {b.min = b.max = smin; b.discrete=false; return b;}
        b.min = coder[1].read_int2(smin, smax);
        b.max = coder[2].read_int2(b.min, smax);
        if (b.min == b.max) { b.discrete=false; return b; }
        if (b.min + 1 == b.max) { b.discrete=false; return b; }
        b.discrete = coder[3].read_int2(0,1);
        if (b.discrete) {
           int nb = coder[4].read_int2(2, std::min((int)max_per_colorbucket[plane],b.max-b.min));
           b.values.push_back(b.min);
           ColorVal v=b.min;
             for (int p=1; p < nb-1; p++) {
               b.values.push_back(coder[5].read_int2(v+1, b.max+1-nb+p));
               v = b.values[p];
             }
           if (b.min < b.max) b.values.push_back(b.max);
        }
        return b;
    }
    bool load(const ColorRanges *srcRanges, RacIn<IO> &rac) override {
//        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18> coder(rac);
        std::vector<SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18>> coders(6,SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18>(rac));
        prevPlanes pixelL, pixelU;
        cb->bucket0 = load_bucket(coders, srcRanges, 0, pixelL, pixelU);
        pixelL.push_back(cb->min0);
        pixelU.push_back(cb->min0+CB0a-1);
        for (ColorBucket& b : cb->bucket1) {b=load_bucket(coders, srcRanges, 1, pixelL, pixelU); pixelL[0] += CB0a; pixelU[0] += CB0a; }
        if (srcRanges->min(2) < srcRanges->max(2)) {
          pixelL[0] = cb->min0;
          pixelU[0] = cb->min0+CB0b-1;
          pixelL.push_back(cb->min1);
          pixelU.push_back(cb->min1+CB1-1);
          for (auto& bv : cb->bucket2) {
                pixelL[1] = cb->min1;
                pixelU[1] = cb->min1+CB1-1;
                for (ColorBucket& b : bv) {
                        b=load_bucket(coders, srcRanges, 2, pixelL, pixelU);
                        pixelL[1] += CB1; pixelU[1] += CB1;
                }
                pixelL[0] += CB0b; pixelU[0] += CB0b;
          }
        }
        if (srcRanges->numPlanes() > 3) cb->bucket3 = load_bucket(coders, srcRanges, 3, pixelL, pixelU);
        return true;
    }

#ifdef HAS_ENCODER
    void save_bucket(const ColorBucket &b, std::vector<SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18>> &coder, const ColorRanges *srcRanges, const int plane, const prevPlanes &pixelL, const prevPlanes &pixelU) const {
        if (plane<3)
        for (int p=0; p<plane; p++) {
                if (!cb->exists(p,pixelL,pixelU)) {
                        if (!b.empty()) {printf("\nBucket does not exist but is not empty!\n"); assert(false);}
                        return;
                }
        }
        ColorVal smin,smax;
        minmax(srcRanges,plane,pixelL,pixelU,smin,smax);

        if (b.min > b.max) {
                coder[0].write_int2(0, 1, 0);  // empty bucket
                return;
        } else coder[0].write_int2(0, 1, 1);  // non-empty bucket
        if (smin==smax) { return;}


        coder[1].write_int2(smin, smax, b.min);
        coder[2].write_int2(b.min, smax, b.max);
        if (b.min == b.max) return;  // singleton bucket
        if (b.min + 1  == b.max) return; // bucket contains two consecutive values
        coder[3].write_int2(0, 1, b.discrete);
        if (b.discrete) {
           assert((int)b.values.size() < b.max-b.min+1); // no discrete buckets that are completely full
           coder[4].write_int2(2, std::min((int)max_per_colorbucket[plane],b.max-b.min), b.values.size());
           ColorVal v=b.min;
           int nb = b.values.size();
           for (int p=1; p < nb - 1; p++) {
               coder[5].write_int2(v+1, b.max+1-nb+p, b.values[p]);
               v = b.values[p];
           }
        }
    }
    void save(const ColorRanges *srcRanges, RacOut<IO> &rac) const override {
        std::vector<SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18>> coders(6,SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18>(rac));
        //SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18> coder(rac);
//        printf("Saving Y Color Bucket: ");
        prevPlanes pixelL, pixelU;
        save_bucket(cb->bucket0, coders, srcRanges, 0, pixelL, pixelU);
//        printf("\nSaving I Color Buckets\n  ");
        pixelL.push_back(cb->min0);
        pixelU.push_back(cb->min0+CB0a-1);
        for (auto& b : cb->bucket1) { save_bucket(b, coders, srcRanges, 1, pixelL, pixelU); pixelL[0] += CB0a; pixelU[0] += CB0a; }
//        printf("\nSaving Q Color Buckets\n  ");
        if (srcRanges->min(2) < srcRanges->max(2)) {
          pixelL[0] = cb->min0;
          pixelU[0] = cb->min0+CB0b-1;
          pixelL.push_back(cb->min1);
          pixelU.push_back(cb->min1+CB1-1);
          for (auto& bv : cb->bucket2) {
                pixelL[1] = cb->min1;
                pixelU[1] = cb->min1+CB1-1;
                for (auto& b : bv) {
                        save_bucket(b, coders, srcRanges, 2, pixelL, pixelU);
                        pixelL[1] += CB1; pixelU[1] += CB1;
                }
                pixelL[0] += CB0b; pixelU[0] += CB0b;
          }
        }
//        printf("\n");
        if (srcRanges->numPlanes() > 3) {
//          printf("Saving Alpha Color Bucket: ");
          save_bucket(cb->bucket3, coders, srcRanges, 3, pixelL, pixelU);
//          printf("\n");
        }
    }

    bool process(const ColorRanges *srcRanges, const Images &images) override {
            std::vector<ColorVal> pixel(images[0].numPlanes());
            // fill buckets
            for (const Image& image : images)
            for (uint32_t r=0; r<image.rows(); r++) {
                for (uint32_t c=0; c<image.cols(); c++) {
                  int p;
                  for (p=0; p<image.numPlanes(); p++) {
                    ColorVal v = image(p,r,c);
                    pixel[p] = v;
                  }
                  if (image.alpha_zero_special && p>3 && pixel[3]==0) { cb->findBucket(3, pixel).addColor(0,max_per_colorbucket[3]); continue;}
                  cb->addColor(pixel);
                }
            }

            cb->bucket0.simplify_lossless();
            cb->bucket3.simplify_lossless();
            for (auto& b : cb->bucket1) b.simplify_lossless();
            for (auto& bv : cb->bucket2) for (auto& b : bv) b.simplify_lossless();


        // TODO: IMPROVE THESE HEURISTICS!
        // TAKE IMAGE SIZE INTO ACCOUNT!
        // CONSIDER RELATIVE AREA OF BUCKETS / BOUNDS!

//            printf("Filled color buckets with %i discrete colors + %i continous buckets\n",totaldiscretecolors,totalcontinuousbuckets);

            int64_t total_pixels = (int64_t) images.size() * images[0].rows() * images[0].cols();
            v_printf(7,", [D=%i,C=%i,P=%i]",totaldiscretecolors,totalcontinuousbuckets,(int) (total_pixels/100));
            if (total_pixels > 5000000) total_pixels = 5000000; // let's discourage using ColorBuckets just because the image is big
            if (totaldiscretecolors < total_pixels/200 && totalcontinuousbuckets < total_pixels/50) return true;
            if (totaldiscretecolors < total_pixels/100 && totalcontinuousbuckets < total_pixels/200) return true;
            if (totaldiscretecolors < total_pixels/40 && totalcontinuousbuckets < total_pixels/500) return true;

            // simplify buckets
            for (int factor = 95; factor >= 35; factor -= 10) {
                for (auto& b : cb->bucket1) b.simplify(factor);
                for (auto& bv : cb->bucket2) for (auto& b : bv) b.simplify(factor-20);
                v_printf(8,"->[D=%i,C=%i]",totaldiscretecolors,totalcontinuousbuckets);
                if (totaldiscretecolors < total_pixels/200 && totalcontinuousbuckets < total_pixels/100) return true;
            }
            return false;
    }
#endif
};
