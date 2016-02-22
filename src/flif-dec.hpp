#pragma once

template <typename IO>
bool flif_decode(IO& io, Images &images, int quality, int scale, uint32_t (*callback)(int32_t,int64_t), int, Images &partial_images, int rw=0, int rh=0, int crc_check=-1);

template <typename IO>
bool flif_decode(IO& io, Images &images, int quality = 100, int scale = 1, int rw=0, int rh=0, int crc_check=-1) {return flif_decode(io, images, quality, scale, NULL, 0, images, rw, rh, crc_check);}
