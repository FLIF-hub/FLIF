#ifndef FLIF_INTERFACE_H
#define FLIF_INTERFACE_H

#include <stdint.h>
#include <stddef.h>

#ifdef _MSC_VER
#define FLIF_API __cdecl
#else
#define FLIF_API __attribute__((cdecl))
#endif

/*!
* C interface for the FLIF library
*/

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

    typedef struct FLIF_IMAGE FLIF_IMAGE;
    typedef struct FLIF_DECODER FLIF_DECODER;
    typedef struct FLIF_ENCODER FLIF_ENCODER;

    FLIF_IMAGE* FLIF_API flif_create_image(uint32_t width, uint32_t height);
    void FLIF_API flif_destroy_image(FLIF_IMAGE* image);

    uint32_t FLIF_API flif_image_get_width(FLIF_IMAGE* image);
    uint32_t FLIF_API flif_image_get_height(FLIF_IMAGE* image);
    // TODO: tentative
    void FLIF_API flif_image_write_row_RGBA8(FLIF_IMAGE* image, uint32_t row, const void* buffer, size_t buffer_size_bytes);
    // TODO: tentative
    void FLIF_API flif_image_read_row_RGBA8(FLIF_IMAGE* image, uint32_t row, void* buffer, size_t buffer_size_bytes);

    FLIF_DECODER* FLIF_API flif_create_decoder();
    void FLIF_API flif_destroy_decoder(FLIF_DECODER* decoder);

    void FLIF_API flif_decoder_set_quality(FLIF_DECODER* decoder, int32_t quality);
    void FLIF_API flif_decoder_set_scale(FLIF_DECODER* decoder, uint32_t scale);

    int32_t FLIF_API flif_decoder_decode_file(FLIF_DECODER* decoder, const char* filename);
    int32_t FLIF_API flif_decoder_decode_memory(FLIF_DECODER* decoder, const void* buffer, size_t buffer_size_bytes);
    size_t FLIF_API flif_decoder_num_images(FLIF_DECODER* decoder);
    FLIF_IMAGE* FLIF_API flif_decoder_get_image(FLIF_DECODER* decoder, size_t index);

    FLIF_ENCODER* FLIF_API flif_create_encoder();
    void FLIF_API flif_destroy_encoder(FLIF_ENCODER* encoder);

    void FLIF_API flif_encoder_set_interlaced(FLIF_ENCODER* encoder, uint32_t interlaced);
    void FLIF_API flif_encoder_set_learn_repeat(FLIF_ENCODER* encoder, uint32_t learn_repeats);
    void FLIF_API flif_encoder_set_auto_color_buckets(FLIF_ENCODER* encoder, uint32_t acb);
    void FLIF_API flif_encoder_set_frame_delay(FLIF_ENCODER* encoder, uint32_t frame_delay);
    void FLIF_API flif_encoder_set_palette_size(FLIF_ENCODER* encoder, int32_t palette_size);
    void FLIF_API flif_encoder_set_loopback(FLIF_ENCODER* encoder, int32_t loopback);

    void FLIF_API flif_encoder_add_image(FLIF_ENCODER* encoder, FLIF_IMAGE* image);
    int32_t FLIF_API flif_encoder_encode_file(FLIF_ENCODER* encoder, const char* filename);
    int32_t FLIF_API flif_encoder_encode_memory(FLIF_ENCODER* encoder, void** buffer, size_t* buffer_size_bytes);

    void FLIF_API flif_free_memory(void* buffer);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // FLIF_INTERFACE_H
