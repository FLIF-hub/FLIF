#ifndef FLIF_FRAMESHAPE_H
#define FLIF_FRAMESHAPE_H 1

#include <vector>

#include "transform.h"
#include "../maniac/symbol.h"



template <typename IO>
class TransformFrameShape : public Transform<IO> {
protected:
    std::vector<uint32_t> b;
    std::vector<uint32_t> e;
    uint32_t cols;
    uint32_t nb;

    const ColorRanges *meta(Images& images, const ColorRanges *srcRanges) {
        uint32_t pos=0;
        for (unsigned int fr=1; fr<images.size(); fr++) {
            Image& image = images[fr];
            if (image.seen_before >= 0) continue;
            for (uint32_t r=0; r<image.rows(); r++) {
               assert(pos<nb);
               image.col_begin[r] = b[pos];
               image.col_end[r] = e[pos];
               pos++;
            }
        }
        return new DupColorRanges(srcRanges);
    }

    void configure(const int setting) { if (nb==0) nb=setting; else cols=setting; } // ok this is dirty

    void load(const ColorRanges *srcRanges, RacIn<IO> &rac) {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 24> coder(rac);
        for (unsigned int i=0; i<nb; i+=1) {b.push_back(coder.read_int(0,cols));}
        for (unsigned int i=0; i<nb; i+=1) {e.push_back(cols-coder.read_int(0,cols-b[i]));}
//        for (unsigned int i=0; i<nb; i+=1) {e.push_back(coder.read_int(b[i],cols));}
    }

    void save(const ColorRanges *srcRanges, RacOut<IO> &rac) const {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 24> coder(rac);
        assert(nb == b.size());
        assert(nb == e.size());
        for (unsigned int i=0; i<nb; i+=1) { coder.write_int(0,cols,b[i]); }
        for (unsigned int i=0; i<nb; i+=1) { coder.write_int(0,cols-b[i],cols-e[i]); }
//        for (unsigned int i=0; i<nb; i+=1) { coder.write_int(b[i],cols,e[i]); }
    }

    bool process(const ColorRanges *srcRanges, const Images &images) {
        if (images.size()<2) return false;
        int np=srcRanges->numPlanes();
        nb = 0;
        cols = images[0].cols();
        for (unsigned int fr=1; fr<images.size(); fr++) {
            const Image& image = images[fr];
            if (image.seen_before >= 0) continue;
            nb += image.rows();
            for (uint32_t r=0; r<image.rows(); r++) {
                bool beginfound=false;
                for (uint32_t c=0; c<image.cols(); c++) {
                    if (np>3 && image(3,r,c) == 0 && (fr==0 || images[fr-1](3,r,c)==0)) continue;
                    for (int p=0; p<np; p++) {
                       if((fr==0 && image(p,r,c)!=0) || (fr>0 && image(p,r,c) != images[fr-1](p,r,c))) { beginfound=true; break;}
                    }
                    if (beginfound) {b.push_back(c); break;}
                }
                if (!beginfound) {b.push_back(image.cols()); e.push_back(image.cols()); continue;}
                bool endfound=false;
                for (uint32_t c=image.cols()-1; c >= b.back(); c--) {
                    if (np>3 && image(3,r,c) == 0 && (fr==0 || images[fr-1](3,r,c)==0)) continue;
                    for (int p=0; p<np; p++) {
                       if((fr==0 && image(p,r,c)!=0) || (fr>0 && image(p,r,c) != images[fr-1](p,r,c))) { endfound=true; break;}
                    }
                    if (endfound) {e.push_back(c+1); break;}
                }
                if (!endfound) {e.push_back(0); continue;} //shouldn't happen, right?
            }
        }
        /* does not seem to do much good at all
        if (nb&1) {b.push_back(b[nb-1]); e.push_back(e[nb-1]);}
        for (unsigned int i=0; i<nb; i+=2) { b[i]=b[i+1]=std::min(b[i],b[i+1]); }
        for (unsigned int i=0; i<nb; i+=2) { e[i]=e[i+1]=std::max(e[i],e[i+1]); }
        */
        return true;
    }
};

#endif
