#pragma once

template <typename IO>
bool flif_decode(IO& io, Images &images, int quality, int scale, uint32_t (*callback)(int,int), Images &partial_images);

template <typename IO>
bool flif_decode(IO& io, Images &images, int quality = 100, int scale = 1) {return flif_decode(io, images, quality, scale, NULL, images);}
