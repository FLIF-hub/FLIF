#include <sstream>

#include "symbol.hpp"

#ifdef STATS
std::string SymbolChanceStats::format() const {
    std::stringstream ss;
    ss << "0:" << stats_zero.format() << ' ';
    ss << "-:" << stats_sign.format() << ' ';
    for (int i = 0; i < 31; i++) {
        ss << "e" << i << ":" << stats_exp[i].format() << ' ';
    }
    for (int i = 0; i < 32; i++) {
        ss << "m" << i << ":" << stats_mant[i].format() << ' ';
    }
    return ss.str();
}

SymbolChanceStats::~SymbolChanceStats() {
    fprintf(stderr, "STATS: %s\n", format().c_str());
}

SymbolChanceStats global_symbol_stats;
#endif
