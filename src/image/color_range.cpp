#include "color_range.hpp"

const ColorRanges *getRanges(const Image &image) {
    StaticColorRangeList ranges;
    for (int p = 0; p < image.numPlanes(); p++) {
        ranges.push_back(std::make_pair(image.min(p), image.max(p)));
    }
    return new StaticColorRanges(ranges);
}

const ColorRanges *getRanges(const ColorRanges *ranges) {
    return new DupColorRanges(ranges);
}
