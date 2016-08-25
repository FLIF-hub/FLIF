#pragma once

template <typename IO>
bool flif_decode(IO& io, Images &images, int quality, int scale, uint32_t (*callback)(int32_t,int64_t), int, Images &partial_images, int rw, int rh, int crc_check, bool fit, metadata_options &md);

template <typename IO>
bool flif_decode(IO& io, Images &images, int quality, int scale, int rw, int rh, int crc_check, bool fit, metadata_options &md) {
    return flif_decode(io, images, quality, scale, NULL, 0, images, rw, rh, crc_check, fit, md);
}
