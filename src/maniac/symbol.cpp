#include <sstream>

#include "symbol.hpp"

#ifdef STATS
std::string SymbolChanceStats::format() const {
    std::stringstream ss;
    ss << "zero," << stats_zero.format() << ", ";
    ss << "sign," << stats_sign.format() << ", ";
    for (int i = 0; i < 17; i++) {
        ss << "e" << i << "," << stats_exp[i].format() << ", ";
    }
    for (int i = 0; i < 18; i++) {
        ss << "m" << i << "," << stats_mant[i].format() << ", ";
    }
    return ss.str();
}

SymbolChanceStats::~SymbolChanceStats() {
    fprintf(stderr, "STATS: %s\n", format().c_str());
}

SymbolChanceStats global_symbol_stats;
#endif
