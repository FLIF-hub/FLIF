#ifndef FLIF_FRAMEDUP_H
#define FLIF_FRAMEDUP_H 1

#include <vector>

#include "transform.h"
#include "../maniac/symbol.h"



template <typename IO>
class TransformFrameDup : public Transform<IO> {
protected:
    std::vector<int> seen_before;
    uint32_t nb;

    const ColorRanges *meta(Images& images, const ColorRanges *srcRanges) {
        for (unsigned int fr=0; fr<images.size(); fr++) {
            Image& image = images[fr];
            image.seen_before = seen_before[fr];
        }
        return new DupColorRanges(srcRanges);
    }

    void configure(const int setting) { nb=setting; }

    void load(const ColorRanges *srcRanges, RacIn<IO> &rac) {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 24> coder(rac);
        seen_before.clear();
        seen_before.push_back(-1);
        for (unsigned int i=1; i<nb; i++) seen_before.push_back(coder.read_int(-1,nb-2));
        int count=0; for(int i : seen_before) { if(i>=0) count++; } v_printf(5,"[%i]",count);
    }

    void save(const ColorRanges *srcRanges, RacOut<IO> &rac) const {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 24> coder(rac);
        assert(nb == seen_before.size());
        for (unsigned int i=1; i<seen_before.size(); i++) coder.write_int(-1,nb-2,seen_before[i]);
        int count=0; for(int i : seen_before) { if(i>=0) count++; } v_printf(5,"[%i]",count);
    }

    bool process(const ColorRanges *srcRanges, const Images &images) {
        int np=srcRanges->numPlanes();
        nb = images.size();
        seen_before.empty();
        seen_before.resize(nb,-1);
        bool dupes_found=false;
        for (unsigned int fr=1; fr<images.size(); fr++) {
            const Image& image = images[fr];
            for (unsigned int ofr=0; ofr<fr; ofr++) {
              const Image& oimage = images[ofr];
              bool identical=true;
              for (uint32_t r=0; r<image.rows(); r++) {
                for (uint32_t c=0; c<image.cols(); c++) {
                    for (int p=0; p<np; p++) {
                       if(image(p,r,c) != oimage(p,r,c)) { identical=false; break;}
                    }
                    if (!identical) {break;}
                }
                if (!identical) {break;}
              }
              if (identical) {seen_before[fr] = ofr; dupes_found=true; break;}
            }
        }
        return dupes_found;
    }
};

#endif
