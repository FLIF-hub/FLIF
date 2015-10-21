#include "flif-interface-private.h"

FLIF_IMAGE::FLIF_IMAGE()
{
}

#pragma pack(push,1)
struct FLIF_RGBA
{
    uint8_t r,g,b,a;
};
#pragma pack(pop)

void FLIF_IMAGE::write_row_RGBA8(uint32_t row, const void* buffer, size_t buffer_size_bytes)
{
    if(buffer_size_bytes < image.cols() * sizeof(FLIF_RGBA))
        return;

    const FLIF_RGBA* buffer_rgba = reinterpret_cast<const FLIF_RGBA*>(buffer);

    if(image.numPlanes() >= 3)
    {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            image.set(0, row, c, buffer_rgba[c].r);
            image.set(1, row, c, buffer_rgba[c].g);
            image.set(2, row, c, buffer_rgba[c].b);
        }
    }
    if(image.numPlanes() == 4)
    {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            image.set(3, row, c, buffer_rgba[c].a);
        }
    }
}

void FLIF_IMAGE::read_row_RGBA8(uint32_t row, void* buffer, size_t buffer_size_bytes)
{
    if(buffer_size_bytes < image.cols() * sizeof(FLIF_RGBA))
        return;

    FLIF_RGBA* buffer_rgba = reinterpret_cast<FLIF_RGBA*>(buffer);

    if(image.numPlanes() >= 3)
    {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].r = image(0, row, c) & 0xFF;
            buffer_rgba[c].g = image(1, row, c) & 0xFF;
            buffer_rgba[c].b = image(2, row, c) & 0xFF;
        }
    }
    if(image.numPlanes() == 4)
    {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].a = image(3, row, c) & 0xFF;
        }
    }
    else
    {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].a = 0xFF;
        }
    }
}

//=============================================================================

FLIF_DECODER::FLIF_DECODER()
: quality(100)
, scale(1)
, callback(NULL)
{
}

int32_t FLIF_DECODER::decode_file(const char* filename)
{
    internal_images.clear();
    images.clear();
//    requested_images.clear();

    FILE *file = fopen(filename,"rb");
    if(!file)
        return 0;
    FileIO fio(file, filename);

    if(!flif_decode(fio, internal_images, quality, scale, reinterpret_cast<uint32_t (*)(int,int)>(callback), images))
        return 0;

    images.clear();
    for (Image& image : internal_images) images.emplace_back(std::move(image));
//    requested_images.resize(images.size());

    return 1;
}

int32_t FLIF_DECODER::decode_memory(const void* buffer, size_t buffer_size_bytes)
{
    (void)buffer;
    (void)buffer_size_bytes;
    // TODO
    return 0;
}

size_t FLIF_DECODER::num_images()
{
    return images.size();
}

FLIF_IMAGE* FLIF_DECODER::get_image(size_t index)
{
    if(index >= images.size())
        return 0;
    FLIF_IMAGE *i = new FLIF_IMAGE();
    i->image = images[index].clone();
    return i;

//    if(requested_images[index].get() == 0)
//    {
//        requested_images[index].reset(new FLIF_IMAGE());

        // this line invalidates images[index]
        // but we aren't using it afterwards, so it is ok
//        requested_images[index]->image = std::move(images[index]);
//    }

//   return requested_images[index].get();
}

//=============================================================================

FLIF_ENCODER::FLIF_ENCODER()
: interlaced(1)
, learn_repeats(3)
, acb(1)
, frame_delay(100)
, palette_size(512)
, loopback(1)
{
}

void FLIF_ENCODER::add_image(FLIF_IMAGE* image)
{
    images.push_back(image);
}

/*!
* \return non-zero if the function succeeded
*/
int32_t FLIF_ENCODER::encode_file(const char* filename)
{
    FILE *file = fopen(filename,"wb");
    if(!file)
        return 0;
    FileIO fio(file, filename);

    // TODO: need to change flif_encode() so these expensive copies can be avoided
    Images copies;
    for(size_t i = 0; i < images.size(); ++i)
        copies.push_back(images[i]->image.clone());

    std::vector<std::string> transDesc = {"YIQ","BND","PLA","PLT","ACB","DUP","FRS","FRA"};

    if(!flif_encode(fio, copies,
        transDesc,
        interlaced != 0 ? flifEncoding::interlaced : flifEncoding::nonInterlaced,
        learn_repeats,
        acb,
        frame_delay,
        palette_size,
        loopback))
    {
        return 0;
    }

    return 1;
}

/*!
* \return non-zero if the function succeeded
*/
int32_t FLIF_ENCODER::encode_memory(void** buffer, size_t* buffer_size_bytes)
{
    (void)buffer;
    (void)buffer_size_bytes;
    // TODO

    return 0;
}

//=============================================================================

/*!
Notes about the C interface:

Only use types known to C.
Use types that are unambiguous across all compilers, like uint32_t.
Each function must have it's call convention set.
Exceptions must be caught no matter what.

*/

//=============================================================================

#ifdef _MSC_VER
#define FLIF_DLLEXPORT __declspec(dllexport)
#else
#define FLIF_DLLEXPORT __attribute__ ((visibility ("default")))
#endif

extern "C" {

FLIF_DLLEXPORT FLIF_IMAGE* FLIF_API flif_create_image(uint32_t width, uint32_t height)
{
    try
    {
        std::unique_ptr<FLIF_IMAGE> image(new FLIF_IMAGE());
        image->image.init(width, height, 0, 255, 4);
        return image.release();
    }
    catch(...)
    {
    }

    return 0;
}

FLIF_DLLEXPORT void FLIF_API flif_destroy_image(FLIF_IMAGE* image)
{
    // delete should never let exceptions out
    delete image;
}

FLIF_DLLEXPORT uint32_t FLIF_API flif_image_get_width(FLIF_IMAGE* image)
{
    try
    {
        return image->image.cols();
    }
    catch(...)
    {
    }

    return 0;
}

FLIF_DLLEXPORT uint32_t FLIF_API flif_image_get_height(FLIF_IMAGE* image)
{
    try
    {
        return image->image.rows();
    }
    catch(...)
    {
    }

    return 0;
}

FLIF_DLLEXPORT void FLIF_API flif_image_write_row_RGBA8(FLIF_IMAGE* image, uint32_t row, const void* buffer, size_t buffer_size_bytes)
{
    try
    {
        image->write_row_RGBA8(row, buffer, buffer_size_bytes);
    }
    catch(...)
    {
    }
}

FLIF_DLLEXPORT void FLIF_API flif_image_read_row_RGBA8(FLIF_IMAGE* image, uint32_t row, void* buffer, size_t buffer_size_bytes)
{
    try
    {
        image->read_row_RGBA8(row, buffer, buffer_size_bytes);
    }
    catch(...)
    {
    }
}

//=============================================================================

FLIF_DLLEXPORT FLIF_DECODER* FLIF_API flif_create_decoder()
{
    try
    {
        std::unique_ptr<FLIF_DECODER> decoder(new FLIF_DECODER());
        return decoder.release();
    }
    catch(...)
    {
    }

    return 0;
}

FLIF_DLLEXPORT void FLIF_API flif_destroy_decoder(FLIF_DECODER* decoder)
{
    // delete should never let exceptions out
    delete decoder;
}

FLIF_DLLEXPORT void FLIF_API flif_decoder_set_quality(FLIF_DECODER* decoder, int32_t quality)
{
    try
    {
        decoder->quality = quality;
    }
    catch(...)
    {
    }
}

FLIF_DLLEXPORT void FLIF_API flif_decoder_set_scale(FLIF_DECODER* decoder, uint32_t scale)
{
    try
    {
        decoder->scale = scale;
    }
    catch(...)
    {
    }
}

FLIF_DLLEXPORT void FLIF_API flif_decoder_set_callback(FLIF_DECODER* decoder, uint32_t (*callback)(int quality,int bytes_read))
{
    try
    {
        decoder->callback = (void*) callback;
    }
    catch(...)
    {
    }
}

/*!
* \return non-zero if the function succeeded
*/
FLIF_DLLEXPORT int32_t FLIF_API flif_decoder_decode_file(FLIF_DECODER* decoder, const char* filename)
{
    try
    {
        return decoder->decode_file(filename);
    }
    catch(...)
    {
    }

    return 0;
}

/*!
* \return non-zero if the function succeeded
*/
FLIF_DLLEXPORT int32_t FLIF_API flif_decoder_decode_memory(FLIF_DECODER* decoder, const void* buffer, size_t buffer_size_bytes)
{
    try
    {
        return decoder->decode_memory(buffer, buffer_size_bytes);
    }
    catch(...)
    {
    }

    return 0;
}

FLIF_DLLEXPORT size_t FLIF_API flif_decoder_num_images(FLIF_DECODER* decoder)
{
    try
    {
        return decoder->num_images();
    }
    catch(...)
    {
    }

    return 0;
}

FLIF_DLLEXPORT FLIF_IMAGE* FLIF_API flif_decoder_get_image(FLIF_DECODER* decoder, size_t index)
{
    try
    {
        return decoder->get_image(index);
    }
    catch(...)
    {
    }

    return 0;
}

//=============================================================================

FLIF_DLLEXPORT FLIF_ENCODER* FLIF_API flif_create_encoder()
{
    try
    {
        std::unique_ptr<FLIF_ENCODER> encoder(new FLIF_ENCODER());
        return encoder.release();
    }
    catch(...)
    {
    }

    return 0;
}

FLIF_DLLEXPORT void FLIF_API flif_destroy_encoder(FLIF_ENCODER* encoder)
{
    // delete should never let exceptions out
    delete encoder;
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_set_interlaced(FLIF_ENCODER* encoder, uint32_t interlaced)
{
    try
    {
        encoder->interlaced = interlaced;
    }
    catch(...)
    {
    }
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_set_learn_repeat(FLIF_ENCODER* encoder, uint32_t learn_repeats)
{
    try
    {
        encoder->learn_repeats = learn_repeats;
    }
    catch(...)
    {
    }
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_set_auto_color_buckets(FLIF_ENCODER* encoder, uint32_t acb)
{
    try
    {
        encoder->acb = acb;
    }
    catch(...)
    {
    }
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_set_frame_delay(FLIF_ENCODER* encoder, uint32_t frame_delay)
{
    try
    {
        encoder->frame_delay = frame_delay;
    }
    catch(...)
    {
    }
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_set_palette_size(FLIF_ENCODER* encoder, int32_t palette_size)
{
    try
    {
        encoder->palette_size = palette_size;
    }
    catch(...)
    {
    }
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_set_loopback(FLIF_ENCODER* encoder, int32_t loopback)
{
    try
    {
        encoder->loopback = loopback;
    }
    catch(...)
    {
    }
}

FLIF_DLLEXPORT void FLIF_API flif_encoder_add_image(FLIF_ENCODER* encoder, FLIF_IMAGE* image)
{
    try
    {
        encoder->add_image(image);
    }
    catch(...)
    {
    }
}

/*!
* \return non-zero if the function succeeded
*/
FLIF_DLLEXPORT int32_t FLIF_API flif_encoder_encode_file(FLIF_ENCODER* encoder, const char* filename)
{
    try
    {
        return encoder->encode_file(filename);
    }
    catch(...)
    {
    }

    return 0;
}

/*!
* \return non-zero if the function succeeded
*/
FLIF_DLLEXPORT int32_t FLIF_API flif_encoder_encode_memory(FLIF_ENCODER* encoder, void** buffer, size_t* buffer_size_bytes)
{
    try
    {
        return encoder->encode_memory(buffer, buffer_size_bytes);
    }
    catch(...)
    {
    }

    return 0;
}

FLIF_DLLEXPORT void FLIF_API flif_free_memory(void* buffer)
{
    delete [] reinterpret_cast<uint8_t*>(buffer);
}

} // extern "C"
