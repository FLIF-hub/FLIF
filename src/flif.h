#ifndef FLIF_INTERFACE_H
#define FLIF_INTERFACE_H

#include <stdint.h>
#include <stddef.h>

#ifdef _MSC_VER
#define FLIF_API __cdecl
 #ifdef FLIF_BUILD_DLL
  #define FLIF_DLLIMPORT __declspec(dllexport)
 #elif FLIF_USE_DLL
  #define FLIF_DLLIMPORT __declspec(dllimport)
 #endif
#else
// is this needed?
#define FLIF_API
//#define FLIF_API __attribute__((cdecl))
#endif

#ifndef FLIF_DLLIMPORT
#define FLIF_DLLIMPORT
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

    FLIF_DLLIMPORT FLIF_IMAGE* FLIF_API flif_create_image(uint32_t width, uint32_t height);
    FLIF_DLLIMPORT void FLIF_API flif_destroy_image(FLIF_IMAGE* image);

    FLIF_DLLIMPORT uint32_t FLIF_API flif_image_get_width(FLIF_IMAGE* image);
    FLIF_DLLIMPORT uint32_t FLIF_API flif_image_get_height(FLIF_IMAGE* image);
    FLIF_DLLIMPORT uint8_t  FLIF_API flif_image_get_nb_channels(FLIF_IMAGE* image);
    FLIF_DLLIMPORT uint32_t FLIF_API flif_image_get_frame_delay(FLIF_IMAGE* image);
    // TODO: tentative
    FLIF_DLLIMPORT void FLIF_API flif_image_write_row_RGBA8(FLIF_IMAGE* image, uint32_t row, const void* buffer, size_t buffer_size_bytes);
    // TODO: tentative
    FLIF_DLLIMPORT void FLIF_API flif_image_read_row_RGBA8(FLIF_IMAGE* image, uint32_t row, void* buffer, size_t buffer_size_bytes);

    FLIF_DLLIMPORT FLIF_DECODER* FLIF_API flif_create_decoder();
    FLIF_DLLIMPORT void FLIF_API flif_destroy_decoder(FLIF_DECODER* decoder);

    FLIF_DLLIMPORT void FLIF_API flif_decoder_set_quality(FLIF_DECODER* decoder, int32_t quality);
    FLIF_DLLIMPORT void FLIF_API flif_decoder_set_scale(FLIF_DECODER* decoder, uint32_t scale);
    FLIF_DLLIMPORT void FLIF_API flif_decoder_set_callback(FLIF_DECODER* decoder, uint32_t (*callback)(int quality,int bytes_read));

    FLIF_DLLIMPORT int32_t FLIF_API flif_decoder_decode_file(FLIF_DECODER* decoder, const char* filename);
    FLIF_DLLIMPORT int32_t FLIF_API flif_decoder_decode_memory(FLIF_DECODER* decoder, const void* buffer, size_t buffer_size_bytes);
    FLIF_DLLIMPORT size_t FLIF_API flif_decoder_num_images(FLIF_DECODER* decoder);
    FLIF_DLLIMPORT FLIF_IMAGE* FLIF_API flif_decoder_get_image(FLIF_DECODER* decoder, size_t index);

    FLIF_DLLIMPORT FLIF_ENCODER* FLIF_API flif_create_encoder();
    FLIF_DLLIMPORT void FLIF_API flif_destroy_encoder(FLIF_ENCODER* encoder);

    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_interlaced(FLIF_ENCODER* encoder, uint32_t interlaced);
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_learn_repeat(FLIF_ENCODER* encoder, uint32_t learn_repeats);
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_auto_color_buckets(FLIF_ENCODER* encoder, uint32_t acb);
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_palette_size(FLIF_ENCODER* encoder, int32_t palette_size);
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_lookback(FLIF_ENCODER* encoder, int32_t loopback);
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_divisor(FLIF_ENCODER* encoder, int32_t divisor);
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_min_size(FLIF_ENCODER* encoder, int32_t min_size);
    FLIF_DLLIMPORT void FLIF_API flif_encoder_set_split_threshold(FLIF_ENCODER* encoder, int32_t split_threshold);

    FLIF_DLLIMPORT void FLIF_API flif_encoder_add_image(FLIF_ENCODER* encoder, FLIF_IMAGE* image);
    FLIF_DLLIMPORT int32_t FLIF_API flif_encoder_encode_file(FLIF_ENCODER* encoder, const char* filename);
    FLIF_DLLIMPORT int32_t FLIF_API flif_encoder_encode_memory(FLIF_ENCODER* encoder, void** buffer, size_t* buffer_size_bytes);

    FLIF_DLLIMPORT void FLIF_API flif_free_memory(void* buffer);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // FLIF_INTERFACE_H
