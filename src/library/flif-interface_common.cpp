/*
FLIF - Free Lossless Image Format

Copyright 2010-2016, Jon Sneyers & Pieter Wuille

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once
#include "flif-interface-private_common.hpp"

FLIF_IMAGE::FLIF_IMAGE() { }

#pragma pack(push,1)
struct FLIF_RGB {
	uint8_t r, g, b;
};
struct FLIF_RGBA {
    uint8_t r,g,b,a;
};
struct FLIF_RGBA16 {
    uint16_t r,g,b,a;
};
#pragma pack(pop)

void FLIF_IMAGE::write_row_RGBA8(uint32_t row, const void* buffer, size_t buffer_size_bytes) {
    if(buffer_size_bytes < image.cols() * sizeof(FLIF_RGBA))
        return;

    const FLIF_RGBA* buffer_rgba = reinterpret_cast<const FLIF_RGBA*>(buffer);

    if(image.numPlanes() >= 3) {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            image.set(0, row, c, buffer_rgba[c].r);
            image.set(1, row, c, buffer_rgba[c].g);
            image.set(2, row, c, buffer_rgba[c].b);
        }
    }
    if(image.numPlanes() >= 4) {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            image.set(3, row, c, buffer_rgba[c].a);
        }
    }
}

void FLIF_IMAGE::write_row_RGB8(uint32_t row, const void * buffer, size_t buffer_size_bytes)
{
	if (buffer_size_bytes < image.cols() * sizeof(FLIF_RGB))
		return;

	const FLIF_RGB* buffer_rgb = reinterpret_cast<const FLIF_RGB*>(buffer);

	if (image.numPlanes() >= 3) {
		for (size_t c = 0; c < (size_t)image.cols(); c++) {
			image.set(0, row, c, buffer_rgb[c].r);
			image.set(1, row, c, buffer_rgb[c].g);
			image.set(2, row, c, buffer_rgb[c].b);
		}
	}
	if (image.numPlanes() >= 4) {
		for (size_t c = 0; c < (size_t)image.cols(); c++) {
			image.set(3, row, c, 0xFF); // fully opaque
		}
	}
}

void FLIF_IMAGE::write_row_GRAY8(uint32_t row, const void * buffer, size_t buffer_size_bytes)
{
	if (buffer_size_bytes < image.cols() * sizeof(uint8_t))
		return;

	const uint8_t* buffer_gray = reinterpret_cast<const uint8_t*>(buffer);

	if (image.numPlanes() >= 1) {
		for (size_t c = 0; c < (size_t)image.cols(); c++) {
			image.set(0, row, c, buffer_gray[c]);
		}
	}
	if (image.numPlanes() >= 3) {
		for (size_t c = 0; c < (size_t)image.cols(); c++) {
			image.set(1, row, c, buffer_gray[c]);
			image.set(2, row, c, buffer_gray[c]);
		}
	}
	if (image.numPlanes() >= 4) {
		for (size_t c = 0; c < (size_t)image.cols(); c++) {
			image.set(3, row, c, 0xFF); // fully opaque
		}
	}
}

void FLIF_IMAGE::write_row_PALETTE8(uint32_t row, const void * buffer, size_t buffer_size_bytes)
{
	if (buffer_size_bytes < image.cols() * sizeof(uint8_t))
		return;

	const uint8_t* buffer_gray = reinterpret_cast<const uint8_t*>(buffer);

	if (image.numPlanes() >= 4) {
		for (size_t c = 0; c < (size_t)image.cols(); c++) {
			image.set(0, row, c, 0);
			image.set(1, row, c, buffer_gray[c]);
			image.set(2, row, c, 0);
			image.set(3, row, c, 1);
		}
	}
}

void FLIF_IMAGE::read_row_RGBA8(uint32_t row, void* buffer, size_t buffer_size_bytes) {
    if(buffer_size_bytes < image.cols() * sizeof(FLIF_RGBA))
        return;

    FLIF_RGBA* buffer_rgba = reinterpret_cast<FLIF_RGBA*>(buffer);
    int rshift = 0;
    int mult = 1;
    ColorVal m=image.max(0);
    while (m > 0xFF) { rshift++; m = m >> 1; } // in case the image has bit depth higher than 8
    if ((m != 0) && m < 0xFF) mult = 0xFF / m;
    if (image.palette) {
      assert(image.numPlanes() >= 3);
      // always color
      for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].r = ((image.palette_image->operator()(0, 0, image(1, row, c)) >> rshift) * mult) & 0xFF;
            buffer_rgba[c].g = ((image.palette_image->operator()(1, 0, image(1, row, c)) >> rshift) * mult) & 0xFF;
            buffer_rgba[c].b = ((image.palette_image->operator()(2, 0, image(1, row, c)) >> rshift) * mult) & 0xFF;
      }
      if (image.numPlanes() >= 4) {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].a = ((image.palette_image->operator()(3, 0, image(1, row, c)) >> rshift) * mult) & 0xFF;
        }
      } else {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].a = 0xFF;  // fully opaque
        }
      }
    } else {
      if (image.numPlanes() >= 3) {
        // color
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].r = ((image(0, row, c) >> rshift) * mult) & 0xFF;
            buffer_rgba[c].g = ((image(1, row, c) >> rshift) * mult) & 0xFF;
            buffer_rgba[c].b = ((image(2, row, c) >> rshift) * mult) & 0xFF;
        }
      } else {
        // grayscale
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].r =
            buffer_rgba[c].g =
            buffer_rgba[c].b = ((image(0, row, c) >> rshift) * mult) & 0xFF;
        }
      }
      if (image.numPlanes() >= 4) {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].a = ((image(3, row, c) >> rshift) * mult) & 0xFF;
        }
      } else {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].a = 0xFF;  // fully opaque
        }
      }
    }
}

void FLIF_IMAGE::read_row_GRAY8(uint32_t row, void* buffer, size_t buffer_size_bytes) {
    if(buffer_size_bytes < image.cols()) return;

    uint8_t* buffer_gray = reinterpret_cast<uint8_t*>(buffer);
    int rshift = 0;
    int mult = 1;
    ColorVal m=image.max(0);
    while (m > 0xFF) { rshift++; m = m >> 1; } // in case the image has bit depth higher than 8
    if ((m != 0) && m < 0xFF) mult = 0xFF / m;

    for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_gray[c] = ((image(0, row, c) >> rshift) * mult) & 0xFF;
    }
}

void FLIF_IMAGE::read_row_PALETTE8(uint32_t row, void* buffer, size_t buffer_size_bytes) {
    if(buffer_size_bytes < image.cols()) return;

    assert(image.palette);

    uint8_t* buffer_gray = reinterpret_cast<uint8_t*>(buffer);
    for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_gray[c] = image(1, row, c) & 0xFF;
    }
}


void FLIF_IMAGE::write_row_RGBA16(uint32_t row, const void* buffer, size_t buffer_size_bytes) {
    if(buffer_size_bytes < image.cols() * sizeof(FLIF_RGBA16))
        return;

    const FLIF_RGBA16* buffer_rgba = reinterpret_cast<const FLIF_RGBA16*>(buffer);

    if(image.numPlanes() >= 3) {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            image.set(0, row, c, buffer_rgba[c].r);
            image.set(1, row, c, buffer_rgba[c].g);
            image.set(2, row, c, buffer_rgba[c].b);
        }
    }
    if(image.numPlanes() >= 4) {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            image.set(3, row, c, buffer_rgba[c].a);
        }
    }
}

void FLIF_IMAGE::read_row_RGBA16(uint32_t row, void* buffer, size_t buffer_size_bytes) {
    if(buffer_size_bytes < image.cols() * sizeof(FLIF_RGBA16))
        return;

    FLIF_RGBA16* buffer_rgba = reinterpret_cast<FLIF_RGBA16*>(buffer);
    int rshift = 0;
    int mult = 1;
    ColorVal m=image.max(0);
    while (m > 0xFFFF) { rshift++; m = m >> 1; } // in the unlikely case that the image has bit depth higher than 16
    if ((m != 0) && m < 0xFFFF) mult = 0xFFFF / m;

    if(image.numPlanes() >= 3) {
        // color
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].r = (image(0, row, c) >> rshift) * mult;
            buffer_rgba[c].g = (image(1, row, c) >> rshift) * mult;
            buffer_rgba[c].b = (image(2, row, c) >> rshift) * mult;
        }
    } else {
        // grayscale
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].r =
            buffer_rgba[c].g =
            buffer_rgba[c].b = (image(0, row, c) >> rshift) * mult;
        }
    }
    if(image.numPlanes() >= 4) {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].a = (image(3, row, c) >> rshift) * mult;
        }
    } else {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].a = 0xFFFF;  // fully opaque
        }
    }
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


extern "C" {

FLIF_DLLEXPORT FLIF_IMAGE* FLIF_API flif_create_image(uint32_t width, uint32_t height) {
    try
    {
        std::unique_ptr<FLIF_IMAGE> image(new FLIF_IMAGE());
        image->image.init(width, height, 0, 255, 4);
        return image.release();
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT FLIF_IMAGE* FLIF_API flif_create_image_RGB(uint32_t width, uint32_t height) {
    try
    {
        std::unique_ptr<FLIF_IMAGE> image(new FLIF_IMAGE());
        image->image.init(width, height, 0, 255, 3);
        return image.release();
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT FLIF_IMAGE* FLIF_API flif_create_image_GRAY(uint32_t width, uint32_t height) {
    try
    {
        std::unique_ptr<FLIF_IMAGE> image(new FLIF_IMAGE());
        image->image.init(width, height, 0, 255, 1);
        return image.release();
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT FLIF_IMAGE* FLIF_API flif_create_image_PALETTE(uint32_t width, uint32_t height) {
    try
    {
        std::unique_ptr<FLIF_IMAGE> image(new FLIF_IMAGE());
        image->image.semi_init(width, height, 0, 255, 4);
        image->image.make_constant_plane(0,0);
        image->image.make_constant_plane(2,0);
        image->image.make_constant_plane(3,1);
        image->image.real_init(true);
        image->image.palette = true;
        return image.release();
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT FLIF_IMAGE* FLIF_API flif_create_image_HDR(uint32_t width, uint32_t height) {
    try
    {
        std::unique_ptr<FLIF_IMAGE> image(new FLIF_IMAGE());
#ifdef SUPPORT_HDR
        image->image.init(width, height, 0, 65535, 4);
#else
        image->image.init(width, height, 0, 255, 4);
#endif
        return image.release();
    }
    catch(...) {}
    return 0;
}

FLIF_DLLIMPORT FLIF_IMAGE* FLIF_API flif_import_image_RGBA(uint32_t width, uint32_t height, const void* rgba, uint32_t rgba_stride) {
	try
	{
		const int number_components = 4;
		if (width == 0 || height == 0 || rgba_stride < width*number_components)
			return 0;
		std::unique_ptr<FLIF_IMAGE> image(new FLIF_IMAGE());
		image->image.init(width, height, 0, 255, number_components);
		const uint8_t* buffer = static_cast<const uint8_t*>(rgba);
		for (uint32_t row = 0; row < height; ++row) {
			image->write_row_RGBA8(row, buffer, width*number_components);
			buffer += rgba_stride;
		}
		return image.release();
	}
	catch (...) {}
	return 0;
}

FLIF_DLLIMPORT FLIF_IMAGE* FLIF_API flif_import_image_RGB(uint32_t width, uint32_t height, const void* rgb, uint32_t rgb_stride) {
	try
	{
		const int number_components = 3;
		if (width == 0 || height == 0 || rgb_stride < width*number_components)
			return 0;
		std::unique_ptr<FLIF_IMAGE> image(new FLIF_IMAGE());
		image->image.init(width, height, 0, 255, number_components);
		const uint8_t* buffer = static_cast<const uint8_t*>(rgb);
		for (uint32_t row = 0; row < height; ++row) {
			image->write_row_RGB8(row, buffer, width*number_components);
			buffer += rgb_stride;
		}
		return image.release();
	}
	catch (...) {}
	return 0;
}

FLIF_DLLIMPORT FLIF_IMAGE* FLIF_API flif_import_image_GRAY(uint32_t width, uint32_t height, const void* gray, uint32_t gray_stride) {
	try
	{
		const int number_components = 1;
		if (width == 0 || height == 0 || gray_stride < width*number_components)
			return 0;
		std::unique_ptr<FLIF_IMAGE> image(new FLIF_IMAGE());
		image->image.init(width, height, 0, 255, number_components);
		const uint8_t* buffer = static_cast<const uint8_t*>(gray);
		for (uint32_t row = 0; row < height; ++row) {
			image->write_row_GRAY8(row, buffer, width*number_components);
			buffer += gray_stride;
		}
		return image.release();
	}
	catch (...) {}
	return 0;
}

FLIF_DLLIMPORT FLIF_IMAGE* FLIF_API flif_import_image_PALETTE(uint32_t width, uint32_t height, const void* gray, uint32_t gray_stride) {
	try
	{
		const int number_components = 1;
		if (width == 0 || height == 0 || gray_stride < width*number_components)
			return 0;
		std::unique_ptr<FLIF_IMAGE> image(new FLIF_IMAGE());
		image->image.semi_init(width, height, 0, 255, 4);
		image->image.make_constant_plane(0,0);
		image->image.make_constant_plane(2,0);
		image->image.make_constant_plane(3,1);
		image->image.real_init(true);
		image->image.palette = true;
		const uint8_t* buffer = static_cast<const uint8_t*>(gray);
		for (uint32_t row = 0; row < height; ++row) {
			image->write_row_PALETTE8(row, buffer, width*number_components);
			buffer += gray_stride;
		}
		return image.release();
	}
	catch (...) {}
	return 0;
}

FLIF_DLLEXPORT void FLIF_API flif_destroy_image(FLIF_IMAGE* image) {
    // delete should never let exceptions out
    delete image;
}

FLIF_DLLEXPORT uint32_t FLIF_API flif_image_get_width(FLIF_IMAGE* image) {
    try
    {
        return image->image.cols();
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT uint32_t FLIF_API flif_image_get_height(FLIF_IMAGE* image) {
    try
    {
        return image->image.rows();
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT uint8_t FLIF_API flif_image_get_depth(FLIF_IMAGE* image) {
    try
    {
        return ( image->image.max(0) > 0xFF ? 16 : 8 );
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT uint8_t FLIF_API flif_image_get_nb_channels(FLIF_IMAGE* image) {
    try
    {
        int nb = image->image.numPlanes();
        if (nb > 4) nb = 4; // there could be an extra plane for FRA
        return nb;
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT uint32_t FLIF_API flif_image_get_palette_size(FLIF_IMAGE* image) {
    try
    {
        uint32_t nb = 0;
        if (image->image.palette && image->image.palette_image) nb = image->image.palette_image->cols();
        return nb;
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT void FLIF_API flif_image_get_palette(FLIF_IMAGE* image, void* buffer) {
    try
    {
        int nb = 0;
        if (image->image.palette && image->image.palette_image) nb = image->image.palette_image->cols();
        FLIF_RGBA* buffer_rgba = reinterpret_cast<FLIF_RGBA*>(buffer);
        for (int i=0; i<nb; i++) {
            buffer_rgba[i].r = image->image.palette_image->operator()(0,0,i);
            buffer_rgba[i].g = image->image.palette_image->operator()(1,0,i);
            buffer_rgba[i].b = image->image.palette_image->operator()(2,0,i);
            if(image->image.numPlanes() >= 4) {
              buffer_rgba[i].a = image->image.palette_image->operator()(3,0,i);
            } else {
              buffer_rgba[i].a = 255;
            }
        }
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_image_set_palette(FLIF_IMAGE* image, const void* buffer, uint32_t palette_size) {
    try
    {
        int nb = palette_size;
        image->image.palette = true;
        if (image->image.palette_image) delete image->image.palette_image;
        image->image.palette_image = new Image(nb,1,0,255,4);
        const FLIF_RGBA* buffer_rgba = reinterpret_cast<const FLIF_RGBA*>(buffer);
        for (int i=0; i<nb; i++) {
            image->image.palette_image->set(0,0,i,buffer_rgba[i].r);
            image->image.palette_image->set(1,0,i,buffer_rgba[i].g);
            image->image.palette_image->set(2,0,i,buffer_rgba[i].b);
            image->image.palette_image->set(3,0,i,buffer_rgba[i].a);
        }
    }
    catch(...) {}
}

FLIF_DLLEXPORT uint32_t FLIF_API flif_image_get_frame_delay(FLIF_IMAGE* image) {
    try
    {
        return image->image.frame_delay;
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT void FLIF_API flif_image_set_frame_delay(FLIF_IMAGE* image, uint32_t delay) {
    try
    {
        image->image.frame_delay = delay;
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_image_set_metadata(FLIF_IMAGE* image, const char* chunkname, const unsigned char* data, size_t length) {
    try
    {
        image->image.set_metadata(chunkname, data, length);
    }
    catch(...) {}
}

FLIF_DLLIMPORT uint8_t FLIF_API flif_image_get_metadata(FLIF_IMAGE* image, const char* chunkname, unsigned char** data, size_t* length){
    uint8_t ret = 0;
    try
    {
        ret = image->image.get_metadata(chunkname, data, length);
    }
    catch(...) {}

    return ret;
}

FLIF_DLLIMPORT void FLIF_API flif_image_free_metadata(FLIF_IMAGE* image, unsigned char* data){
    try
    {
        image->image.free_metadata(data);
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_image_write_row_RGBA8(FLIF_IMAGE* image, uint32_t row, const void* buffer, size_t buffer_size_bytes) {
    try
    {
        image->write_row_RGBA8(row, buffer, buffer_size_bytes);
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_image_read_row_RGBA8(FLIF_IMAGE* image, uint32_t row, void* buffer, size_t buffer_size_bytes) {
    try
    {
        image->read_row_RGBA8(row, buffer, buffer_size_bytes);
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_image_write_row_GRAY8(FLIF_IMAGE* image, uint32_t row, const void* buffer, size_t buffer_size_bytes) {
    try
    {
        image->write_row_GRAY8(row, buffer, buffer_size_bytes);
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_image_read_row_GRAY8(FLIF_IMAGE* image, uint32_t row, void* buffer, size_t buffer_size_bytes) {
    try
    {
        image->read_row_GRAY8(row, buffer, buffer_size_bytes);
    }
    catch(...) {}
}
FLIF_DLLEXPORT void FLIF_API flif_image_write_row_PALETTE8(FLIF_IMAGE* image, uint32_t row, const void* buffer, size_t buffer_size_bytes) {
    try
    {
        image->write_row_PALETTE8(row, buffer, buffer_size_bytes);
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_image_read_row_PALETTE8(FLIF_IMAGE* image, uint32_t row, void* buffer, size_t buffer_size_bytes) {
    try
    {
        image->read_row_PALETTE8(row, buffer, buffer_size_bytes);
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_image_write_row_RGBA16(FLIF_IMAGE* image, uint32_t row, const void* buffer, size_t buffer_size_bytes) {
    try
    {
        image->write_row_RGBA16(row, buffer, buffer_size_bytes);
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_image_read_row_RGBA16(FLIF_IMAGE* image, uint32_t row, void* buffer, size_t buffer_size_bytes) {
    try
    {
        image->read_row_RGBA16(row, buffer, buffer_size_bytes);
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_free_memory(void* buffer) {
    delete [] reinterpret_cast<uint8_t*>(buffer);
}

} // extern "C"
