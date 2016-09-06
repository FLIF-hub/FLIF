#pragma once

struct FLIF_INFO
{
    FLIF_INFO();

    uint32_t width;
    uint32_t height;
    uint8_t channels;
    uint8_t bit_depth;
    size_t num_images;
};

/*!
* @param[out] info An info struct to fill. If this is not a null pointer, the decoding will exit after reading the file header.
*/

template <typename IO>
bool flif_decode(IO& io, Images &images, int quality, int scale, uint32_t (*callback)(int32_t,int64_t), int, Images &partial_images, int rw, int rh, int crc_check, bool fit, metadata_options &md, FLIF_INFO* info);

template <typename IO>
bool flif_decode(IO& io, Images &images, int quality, int scale, int rw, int rh, int crc_check, bool fit, metadata_options &md) {
    return flif_decode(io, images, quality, scale, NULL, 0, images, rw, rh, crc_check, fit, md, 0);
}
