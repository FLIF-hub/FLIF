#pragma once

#include "image/color_range.hpp"
#include "transform/factory.hpp"
#include "common.hpp"

template <typename IO>
bool flif_encode(IO& io, Images &images, const std::vector<std::string> &transDesc, flif_options &options);

template <typename IO>
bool flif_encode(IO& io, Images &images, const std::vector<std::string> &transDesc =
                 {"YCoCg","Bounds","Palette_Alpha","Palette","Color_Buckets","Duplicate_Frame","Frame_Shape","Frame_Lookback"}) {
     flif_options options = FLIF_DEFAULT_OPTIONS;
     return flif_encode(io, images, transDesc, options);
}

