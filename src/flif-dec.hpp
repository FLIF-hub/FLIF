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

typedef uint32_t (*callback_t)(uint32_t quality, int64_t bytes_read, uint8_t decode_over, void *user_data, void *context);

/*!
* @param[out] info An info struct to fill. If this is not a null pointer, the decoding will exit after reading the file header.
*/

template <typename IO>
bool flif_decode(IO& io, Images &images, callback_t callback, void *user_data, int, Images &partial_images, flif_options &options, metadata_options &md, FLIF_INFO* info);

template <typename IO>
bool flif_decode(IO& io, Images &images, flif_options &options, metadata_options &md) {
    return flif_decode(io, images, NULL, NULL, 0, images, options, md, 0);
}
