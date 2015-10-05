#ifndef FLIF_RAC_COMPOUND_H
#define FLIF_RAC_COMPOUND_H 1

#include <vector>
#include <math.h>
#include <stdint.h>
#include "symbol.h"
#include "../image/image.h"

//#define CONTEXT_TREE_SPLIT_THRESHOLD 5461*1
#define CONTEXT_TREE_SPLIT_THRESHOLD 5461*8*5
// 5 byte improvement needed before splitting

#define CONTEXT_TREE_MIN_SUBTREE_SIZE 50
#define CONTEXT_TREE_COUNT_DIV 30

#define CONTEXT_TREE_MIN_COUNT 1
#define CONTEXT_TREE_MAX_COUNT 4096

typedef  ColorVal  PropertyVal;
typedef  std::vector<std::pair<PropertyVal,PropertyVal> > Ranges;
typedef  std::vector<PropertyVal> Properties;

// inner nodes
class PropertyDecisionNode
{
public:
    int8_t property;         // -1 : leaf node, childID unused
    // 0..nb_properties-1 : childID refers to left branch  (in inner_node)
    //                      childID+1 refers to right branch
    PropertyVal splitval;
    uint32_t childID;
    uint32_t leafID;
    int64_t count;
    PropertyDecisionNode(int p=-1, int s=0, int c=0) : property(p), splitval(s), childID(c), leafID(0) {}
};

class Tree : public std::vector<PropertyDecisionNode>
{
protected:
    void print_subtree(FILE* file, int pos, int indent) const {
        const PropertyDecisionNode &n = (*this)[pos];
        for (int i=0; i<2*indent; i++) fputc(' ', file);
        if (n.property == -1)
            fprintf(file, "* leaf id=%i\n", n.leafID);
        else {
            fprintf(file, "* split on prop %i at val %i after %lli steps\n", n.property, n.splitval, (long long int)n.count);
            print_subtree(file, n.childID, indent+1);
            print_subtree(file, n.childID+1, indent+1);
        }
    }
public:
    void print(FILE *file) const {
        print_subtree(file, 0, 0);
    }


    Tree() : std::vector<PropertyDecisionNode>(1, PropertyDecisionNode()) {}
};

// leaf nodes when tree is known
template <typename BitChance, int bits> class FinalCompoundSymbolChances
{
public:
#ifdef STATS
    int64_t s_count;
    double sum;
    double qsum;
#endif
    SymbolChance<BitChance, bits> realChances;

    FinalCompoundSymbolChances() {
#ifdef STATS
        s_count = 0;
        sum = 0.0;
        qsum = 0.0;
#endif
    }

    const SymbolChance<BitChance, bits> &chances() const { return realChances; }

#ifdef STATS
    void info(int n) const {
        std::vector<double> chs;
        realChances.dist(chs);
        indent(n); printf("Chances:\n");
        indent(n+1); printf("Totals ints: %llu\n" , (unsigned long long)s_count);
        indent(n+1); printf("Total bits/int: %.4f [", chs[0]/s_count);
        double bestchs = 1.0/0.0;
        int bestidx = -1;
        for (unsigned int i=1; i<chs.size(); i++) {
            printf("%.4f ", chs[i]/s_count);
            if (chs[i] < bestchs) {
               bestchs = chs[i];
               bestidx = i-1;
            }
        }
        printf("]\n");
//        indent(n+1); printf("Average: %.3f+=%.3f (normal optimal bits/int: %.3f)\n", mu, sigma, nbps);
        indent(n+1); printf("Loss for const scale: %.1f bits (%.1f cnp)\n", bestchs - chs[0], log(bestchs/chs[0])*100.0);
        indent(n+1); printf("Best scale: %i\n", bestidx);
        realChances.info_symbol(n+1);
    }
#endif
};

// leaf nodes during tree construction phase
template <typename BitChance, int bits> class CompoundSymbolChances : public FinalCompoundSymbolChances<BitChance, bits>
{
public:
    std::vector<std::pair<SymbolChance<BitChance, bits>,SymbolChance<BitChance, bits> > > virtChances;
    uint64_t realSize;
    std::vector<uint64_t> virtSize;
    std::vector<int64_t> virtPropSum;
    int64_t count;
    int8_t best_property;

    void resetCounters() {
        best_property = -1;
        realSize = 0;
        count = 0;
        virtPropSum.assign(virtPropSum.size(),0);
        virtSize.assign(virtSize.size(),0);
    }

    CompoundSymbolChances(int nProp) :
        FinalCompoundSymbolChances<BitChance, bits>(),
        virtChances(nProp,std::make_pair(SymbolChance<BitChance, bits>(), SymbolChance<BitChance,bits>())),
        realSize(0),
        virtSize(nProp),
        virtPropSum(nProp),
        count(0),
        best_property(-1)
    { }

};



template <typename BitChance, typename RAC, int bits> class FinalCompoundSymbolBitCoder
{
public:
    typedef typename BitChance::Table Table;

private:
    const Table &table;
    RAC &rac;
    FinalCompoundSymbolChances<BitChance, bits> &chances;

    void inline updateChances(const SymbolChanceBitType type, const int i, bool bit) {
        BitChance& real = chances.realChances.bit(type,i);
        real.put(bit, table);
    }

public:
    FinalCompoundSymbolBitCoder(const Table &tableIn, RAC &racIn, FinalCompoundSymbolChances<BitChance, bits> &chancesIn) : table(tableIn), rac(racIn), chances(chancesIn) {}

    bool inline read(const SymbolChanceBitType type, const int i = 0) {
        BitChance& ch = chances.realChances.bit(type, i);
        bool bit = rac.read(ch.get());
        updateChances(type, i, bit);
        return bit;
    }

    void inline write(const bool bit, const SymbolChanceBitType type, const int i = 0) {
        BitChance& ch = chances.realChances.bit(type, i);
        rac.write(ch.get(), bit);
        updateChances(type, i, bit);
    }
};


template <typename BitChance, typename RAC, int bits> class CompoundSymbolBitCoder
{
public:
    typedef typename BitChance::Table Table;

private:
    const Table &table;
    RAC &rac;
    CompoundSymbolChances<BitChance, bits> &chances;
    std::vector<bool> &select;

    void inline updateChances(SymbolChanceBitType type, int i, bool bit) {
        BitChance& real = chances.realChances.bit(type,i);
        real.estim(bit, chances.realSize);
        real.put(bit, table);

        signed short int best_property = -1;
        uint64_t best_size = chances.realSize;
/*
        if (best_size > (1ULL<<60)) {
                // numbers are getting too large
                printf("Large size counters!\n");
                best_size -= (1ULL<<59);
                for (auto& virt_size : chances.virtSize) virt_size -= (1ULL<<59);
        }
*/
//    fprintf(stdout,"RealSize: %lu ||",best_size);
        for (unsigned int j=0; j<chances.virtChances.size(); j++) {
            BitChance& virt = (select)[j] ? chances.virtChances[j].first.bit(type,i)
                              : chances.virtChances[j].second.bit(type,i);
            virt.estim(bit, chances.virtSize[j]);
            virt.put(bit, table);
            if (chances.virtSize[j] < best_size) {
                best_size = chances.virtSize[j];
                best_property = j;
            }
//      fprintf(stdout,"Virt(%u)Size: %lu ||",j,chances.virtSize[j]);
        }
        chances.best_property = best_property;
//    fprintf(stdout,"\n");
    }
    BitChance inline & bestChance(SymbolChanceBitType type, int i = 0) {
        signed short int p = chances.best_property;
        return (p == -1 ? chances.realChances.bit(type,i)
                : ((select)[p] ? chances.virtChances[p].first.bit(type,i)
                   : chances.virtChances[p].second.bit(type,i) ));
    }

public:
    CompoundSymbolBitCoder(const Table &tableIn, RAC &racIn, CompoundSymbolChances<BitChance, bits> &chancesIn, std::vector<bool> &selectIn) : table(tableIn), rac(racIn), chances(chancesIn), select(selectIn) {}

    bool read(SymbolChanceBitType type, int i = 0) {
        BitChance& ch = bestChance(type, i);
        bool bit = rac.read(ch.get());
        updateChances(type, i, bit);
//    fprintf(stderr,"bit %s%i = %s\n", SymbolChanceBitName[type], i, bit ? "true" : "false");
        return bit;
    }

    void write(bool bit, SymbolChanceBitType type, int i = 0) {
        BitChance& ch = bestChance(type, i);
        rac.write(ch.get(), bit);
        updateChances(type, i, bit);
//    fprintf(stderr,"bit %s%i = %s\n", SymbolChanceBitName[type], i, bit ? "true" : "false");
    }
};


template <typename BitChance, typename RAC, int bits> class FinalCompoundSymbolCoder
{
private:
    typedef typename FinalCompoundSymbolBitCoder<BitChance, RAC, bits>::Table Table;
    RAC &rac;
    const Table table;

public:

    FinalCompoundSymbolCoder(RAC& racIn) : rac(racIn) {}

    int read_int(FinalCompoundSymbolChances<BitChance, bits> &chancesIn, int min, int max) {
        FinalCompoundSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, rac, chancesIn);
        int val = reader<bits>(bitCoder, min, max);
//        printf("%i in %i..%i\n",val, min, max);
        return val;
    }

    void write_int(FinalCompoundSymbolChances<BitChance, bits>& chancesIn, int min, int max, int val) {
#ifdef STATS
        chancesIn.sum += (double)val;
        chancesIn.qsum += (double)val*val;
        chancesIn.s_count++;
#endif
        FinalCompoundSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, rac, chancesIn);
//        printf("%i in %i..%i\n",val, min, max);
        writer<bits>(bitCoder, min, max, val);
    }
    int read_int(FinalCompoundSymbolChances<BitChance, bits> &chancesIn, int nbits) {
        FinalCompoundSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, rac, chancesIn);
        int val = reader(bitCoder, nbits);
        return val;
    }

    void write_int(FinalCompoundSymbolChances<BitChance, bits>& chancesIn, int nbits, int val) {
#ifdef STATS
        chancesIn.sum += (double)val;
        chancesIn.qsum += (double)val*val;
        chancesIn.s_count++;
#endif
        FinalCompoundSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, rac, chancesIn);
        writer(bitCoder, nbits, val);
    }
};

template <typename BitChance, typename RAC, int bits> class CompoundSymbolCoder
{
private:
    typedef typename CompoundSymbolBitCoder<BitChance, RAC, bits>::Table Table;
    RAC &rac;
    const Table table;

public:

    CompoundSymbolCoder(RAC& racIn) : rac(racIn) {}

    int read_int(CompoundSymbolChances<BitChance, bits> &chancesIn, std::vector<bool> &selectIn, int min, int max) {
        if (min == max) { return min; }
        CompoundSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, rac, chancesIn, selectIn);
        return reader<bits>(bitCoder, min, max);
    }

    void write_int(CompoundSymbolChances<BitChance, bits>& chancesIn, std::vector<bool> &selectIn, int min, int max, int val) {
        if (min == max) { assert(val==min); return; }
        CompoundSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, rac, chancesIn, selectIn);
        writer<bits>(bitCoder, min, max, val);
    }
    int read_int(CompoundSymbolChances<BitChance, bits> &chancesIn, std::vector<bool> &selectIn, int nbits) {
        CompoundSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, rac, chancesIn, selectIn);
        return reader(bitCoder, nbits);
    }

    void write_int(CompoundSymbolChances<BitChance, bits>& chancesIn, std::vector<bool> &selectIn, int nbits, int val) {
        CompoundSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, rac, chancesIn, selectIn);
        writer(bitCoder, nbits, val);
    }
};



template <typename BitChance, typename RAC, int bits> class FinalPropertySymbolCoder
{
private:
    FinalCompoundSymbolCoder<BitChance, RAC, bits> coder;
    Ranges range;
    unsigned int nb_properties;
    std::vector<FinalCompoundSymbolChances<BitChance,bits> > leaf_node;
    Tree &inner_node;

    FinalCompoundSymbolChances<BitChance,bits> inline &find_leaf(Properties &properties) {
        Tree::size_type pos = 0;
        while(inner_node[pos].property != -1) {
            if (inner_node[pos].count < 0) {
                if (properties[inner_node[pos].property] > inner_node[pos].splitval) {
                  pos = inner_node[pos].childID;
                } else {
                  pos = inner_node[pos].childID+1;
                }
            } else if (inner_node[pos].count > 0) {
                assert(inner_node[pos].leafID >= 0);
                assert((unsigned int)inner_node[pos].leafID < leaf_node.size());
                inner_node[pos].count--;
                break;
            } else if (inner_node[pos].count == 0) {
                inner_node[pos].count--;
                FinalCompoundSymbolChances<BitChance,bits> &result = leaf_node[inner_node[pos].leafID];
                uint32_t old_leaf = inner_node[pos].leafID;
                uint32_t new_leaf = leaf_node.size();
                FinalCompoundSymbolChances<BitChance,bits> resultCopy = result;
                leaf_node.push_back(resultCopy);
                inner_node[inner_node[pos].childID].leafID = old_leaf;
                inner_node[inner_node[pos].childID+1].leafID = new_leaf;
                if (properties[inner_node[pos].property] > inner_node[pos].splitval) {
                  return leaf_node[old_leaf];
                } else {
                  return leaf_node[new_leaf];
                }
            }
        }
        return leaf_node[inner_node[pos].leafID];
    }

public:
    FinalPropertySymbolCoder(RAC& racIn, Ranges &rangeIn, Tree &treeIn) :
        coder(racIn),
        range(rangeIn),
        nb_properties(range.size()),
        leaf_node(1,FinalCompoundSymbolChances<BitChance,bits>()),
        inner_node(treeIn) 
    {
        inner_node[0].leafID = 0;
    }

    int read_int(Properties &properties, int min, int max) {
        if (min == max) { return min; }
        assert(properties.size() == range.size());
        FinalCompoundSymbolChances<BitChance,bits> &chances = find_leaf(properties);
        return coder.read_int(chances, min, max);
    }

    void write_int(Properties &properties, int min, int max, int val) {
        if (min == max) { assert(val==min); return; }
        assert(properties.size() == range.size());
        FinalCompoundSymbolChances<BitChance,bits> &chances = find_leaf(properties);
        coder.write_int(chances, min, max, val);
    }
    int read_int(Properties &properties, int nbits) {
        assert(properties.size() == range.size());
        FinalCompoundSymbolChances<BitChance,bits> &chances = find_leaf(properties);
        return coder.read_int(chances, nbits);
    }

    void write_int(Properties &properties, int nbits, int val) {
        assert(properties.size() == range.size());
        FinalCompoundSymbolChances<BitChance,bits> &chances = find_leaf(properties);
        coder.write_int(chances, nbits, val);
    }

#ifdef STATS
    void info(int n) const {
        indent(n); printf("Tree:\n");
        for (unsigned int i=0; i<leaf_node.size(); i++) {
            indent(n); printf("Leaf %i\n", i);
            leaf_node[i].info(n+1);
        }
    }
#endif
    void simplify() const {}
};



template <typename BitChance, typename RAC, int bits> class PropertySymbolCoder
{
public:
    typedef CompoundSymbolCoder<BitChance, RAC, bits> Coder;
private:
    RAC &rac;
    Coder coder;
    const Ranges range;
    unsigned int nb_properties;
    std::vector<CompoundSymbolChances<BitChance,bits> > leaf_node;
    Tree &inner_node;
    std::vector<bool> selection;
#ifdef STATS
    uint64_t symbols;
#endif

    CompoundSymbolChances<BitChance,bits> inline &find_leaf(const Properties &properties) {
        uint32_t pos = 0;
        Ranges current_ranges = range;
        while(inner_node[pos].property != -1) {
//        fprintf(stderr,"Checking property %i (val=%i, splitval=%i)\n",inner_node[pos].property,properties[inner_node[pos].property],inner_node[pos].splitval);
            if (properties[inner_node[pos].property] > inner_node[pos].splitval) {
                current_ranges[inner_node[pos].property].first = inner_node[pos].splitval + 1;
                pos = inner_node[pos].childID;
            } else {
                current_ranges[inner_node[pos].property].second = inner_node[pos].splitval;
                pos = inner_node[pos].childID+1;
            }
        }
//    fprintf(stdout,"Returning leaf node %i\n", inner_node[pos].childID);
        CompoundSymbolChances<BitChance,bits> &result = leaf_node[inner_node[pos].leafID];

        // split leaf node if some virtual context is performing (significantly) better
        if(result.best_property != -1
           && result.realSize > result.virtSize[result.best_property] + CONTEXT_TREE_SPLIT_THRESHOLD
           && current_ranges[result.best_property].first < current_ranges[result.best_property].second) {

          int8_t p = result.best_property;
          PropertyVal splitval = result.virtPropSum[p]/result.count;
          if (splitval >= current_ranges[result.best_property].second)
            splitval = current_ranges[result.best_property].second-1; // == does happen because of rounding and running average

          uint32_t new_inner = inner_node.size();
          inner_node.push_back(inner_node[pos]);
          inner_node.push_back(inner_node[pos]);
          inner_node[pos].splitval = splitval;
//            fprintf(stdout,"Splitting on property %i, splitval=%i (count=%i)\n",p,inner_node[pos].splitval, (int)result.count);
          inner_node[pos].property = p;
          inner_node[pos].count = result.count;
          uint32_t new_leaf = leaf_node.size();
          result.resetCounters();
          leaf_node.push_back(CompoundSymbolChances<BitChance,bits>(result));
          uint32_t old_leaf = inner_node[pos].leafID;
          inner_node[pos].childID = new_inner;
          inner_node[new_inner].leafID = old_leaf;
          inner_node[new_inner+1].leafID = new_leaf;
          if (properties[p] > inner_node[pos].splitval) {
                return leaf_node[old_leaf];
          } else {
                return leaf_node[new_leaf];
          }
        }
        return result;
    }

    void inline set_selection_and_update_property_sums(const Properties &properties, CompoundSymbolChances<BitChance,bits> &chances) {
        chances.count++;
        if (chances.count > (1LL<<50)) {
            // numbers are getting dangerously large
            //printf("Reducing big numbers...\n");
            for(unsigned int i=0; i<nb_properties; i++) {
              chances.virtPropSum[i] /= 8;
            }
            chances.count /= 8;
        }
        for(unsigned int i=0; i<nb_properties; i++) {
            assert(properties[i] >= range[i].first);
            assert(properties[i] <= range[i].second);
            chances.virtPropSum[i] += properties[i];
//        fprintf(stdout,"Property %i: %i ||",i,properties[i]);
            selection[i] = (properties[i] > chances.virtPropSum[i]/chances.count);
        }
//    fprintf(stdout,"\n");
    }

public:
    PropertySymbolCoder(RAC& racIn, Ranges &rangeIn, Tree &treeIn) :
        rac(racIn),
        coder(racIn),
        range(rangeIn),
        nb_properties(range.size()),
        leaf_node(1,CompoundSymbolChances<BitChance,bits>(nb_properties)),
        inner_node(treeIn),
        selection(nb_properties,false) {
#ifdef STATS
            symbols = 0;
#endif
    }

    int read_int(Properties &properties, int min, int max) {
#ifdef STATS
        symbols++;
#endif
        CompoundSymbolChances<BitChance,bits> &chances = find_leaf(properties);
        set_selection_and_update_property_sums(properties,chances);
        CompoundSymbolChances<BitChance,bits> &chances2 = find_leaf(properties);
        return coder.read_int(chances2, selection, min, max);
    }

    void write_int(Properties &properties, int min, int max, int val) {
#ifdef STATS
        symbols++;
#endif
        CompoundSymbolChances<BitChance,bits> &chances = find_leaf(properties);
        set_selection_and_update_property_sums(properties,chances);
        CompoundSymbolChances<BitChance,bits> &chances2 = find_leaf(properties);
        coder.write_int(chances2, selection, min, max, val);
    }
    int read_int(Properties &properties, int nbits) {
#ifdef STATS
        symbols++;
#endif
        CompoundSymbolChances<BitChance,bits> &chances = find_leaf(properties);
        set_selection_and_update_property_sums(properties,chances);
        CompoundSymbolChances<BitChance,bits> &chances2 = find_leaf(properties);
        return coder.read_int(chances2, selection, nbits);
    }

    void write_int(Properties &properties, int nbits, int val) {
#ifdef STATS
        symbols++;
#endif
        CompoundSymbolChances<BitChance,bits> &chances = find_leaf(properties);
        set_selection_and_update_property_sums(properties,chances);
        CompoundSymbolChances<BitChance,bits> &chances2 = find_leaf(properties);
        coder.write_int(chances2, selection, nbits, val);
    }

#ifdef STATS
    void info(int n) const {
    }
#endif


    // destructive simplification procedure, prunes subtrees with too low counts
    long long int simplify_subtree(int pos) {
        PropertyDecisionNode &n = inner_node[pos];
        if (n.property == -1) {
//            printf("* leaf %i : count=%lli, size=%llu bits, bits per int: %f\n", n.leafID, (long long int)leaf_node[n.leafID].count, (unsigned long long int)leaf_node[n.leafID].realSize/5461, (leaf_node[n.leafID].count > 0 ? leaf_node[n.leafID].realSize/leaf_node[n.leafID].count*1.0/5461 : -1));
            if (leaf_node[n.leafID].count == 0) return -100; // avoid empty leafs by giving them an extra penalty
            return leaf_node[n.leafID].count;
        } else {
//            printf("* split on prop %i at val %i after %lli steps\n", n.property, n.splitval, (long long int)n.count);
//            printf("* split on prop %i\n", n.property);
            long long int subtree_size = 0;
            subtree_size += simplify_subtree(n.childID);
            subtree_size += simplify_subtree(n.childID+1);
            n.count /= CONTEXT_TREE_COUNT_DIV;
            if (n.count > CONTEXT_TREE_MAX_COUNT) {
//               fprintf(stderr, "Unexpected high count in context tree.\n");
               n.count = CONTEXT_TREE_MAX_COUNT;
            }
            if (n.count < CONTEXT_TREE_MIN_COUNT) n.count=CONTEXT_TREE_MIN_COUNT;
//            printf("%li COUNT\n",n.count);
            if (subtree_size < CONTEXT_TREE_MIN_SUBTREE_SIZE) {
//                printf("  PRUNING THE ABOVE SUBTREE\n");
                n.property = -1; // procedure is destructive because the leafID is not set
            }
            return subtree_size;
        }
    }
    void simplify() {
        simplify_subtree(0);
    }

};



template <typename BitChance, typename RAC> class MetaPropertySymbolCoder
{
public:
    typedef SimpleSymbolCoder<BitChance, RAC, 24> Coder;
private:
    Coder coder;
    const Ranges range;
    unsigned int nb_properties;

public:
    MetaPropertySymbolCoder(RAC &racIn, const Ranges &rangesIn) :
        coder(racIn),
        range(rangesIn),
        nb_properties(rangesIn.size())
    {
        for (unsigned int i=0; i<nb_properties; i++) {
           assert(range[i].first <= range[i].second);
        }
    }
    void write_subtree(int pos, Ranges &subrange, const Tree &tree) {
        const PropertyDecisionNode &n = tree[pos];
        int p = n.property;
        coder.write_int(0,nb_properties,p+1);
        if (p != -1) {
            coder.write_int(CONTEXT_TREE_MIN_COUNT, CONTEXT_TREE_MAX_COUNT, n.count);
//            printf("From properties 0..%i, split node at PROPERTY %i\n",nb_properties-1,p);
            int oldmin = subrange[p].first;
            int oldmax = subrange[p].second;
            assert(oldmin < oldmax);
            coder.write_int(oldmin, oldmax-1, n.splitval);
//            fprintf(stderr, "Pos %i: prop %i splitval %i in [%i..%i]\n", pos, n.property, n.splitval, oldmin, oldmax-1);
            // > splitval
            subrange[p].first = n.splitval+1;
            write_subtree(n.childID, subrange, tree);

            // <= splitval
            subrange[p].first = oldmin;
            subrange[p].second = n.splitval;
            write_subtree(n.childID+1, subrange, tree);

            subrange[p].second = oldmax;
        }
    }
    void write_tree(const Tree &tree) {
          //fprintf(stdout,"Saving tree with %lu nodes.\n",tree.size());
          Ranges rootrange(range);
          write_subtree(0, rootrange, tree);
    }
    int read_subtree(int pos, Ranges &subrange, Tree &tree) {
        PropertyDecisionNode &n = tree[pos];
        int p = n.property = coder.read_int(0,nb_properties)-1;

        if (p != -1) {
            int oldmin = subrange[p].first;
            int oldmax = subrange[p].second;
            if (oldmin >= oldmax) {
              fprintf(stderr, "Invalid tree. Aborting tree decoding.\n");
              return -1;
            }
            n.count = coder.read_int(CONTEXT_TREE_MIN_COUNT, CONTEXT_TREE_MAX_COUNT); // * CONTEXT_TREE_COUNT_QUANTIZATION;
            assert(oldmin < oldmax);
            int splitval = n.splitval = coder.read_int(oldmin, oldmax-1);
            int childID = n.childID = tree.size();
//            fprintf(stderr, "Pos %i: prop %i splitval %i in [%i..%i]\n", pos, n.property, splitval, oldmin, oldmax-1);
            tree.push_back(PropertyDecisionNode());
            tree.push_back(PropertyDecisionNode());
            // > splitval
            subrange[p].first = splitval+1;
            if (read_subtree(childID, subrange, tree) == -1) return -1;

            // <= splitval
            subrange[p].first = oldmin;
            subrange[p].second = splitval;
            if (read_subtree(childID+1, subrange, tree) == -1) return -1;

            subrange[p].second = oldmax;
        }
        return 0;
    }
    void read_tree(Tree &tree) {
          Ranges rootrange(range);
          tree.clear();
          tree.push_back(PropertyDecisionNode());
          read_subtree(0, rootrange, tree);
    }
};

#endif