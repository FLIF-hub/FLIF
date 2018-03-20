/* GdkPixbuf library - FLIF Image Loader
 *
 * Copyright (C) 2011 Alberto Ruiz
 * Copyright (C) 2011 David Mazary
 * Copyright (C) 2014 Přemysl Janouch
 * Copyright (C) 2017 Leo Izen (thebombzen)
 *
 * Authors: Alberto Ruiz <aruiz@gnome.org>
 *          David Mazary <dmaz@vt.edu>
 *          Přemysl Janouch <p.janouch@gmail.com>
 *          Leo Izen (thebombzen) <leo.izen@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Original GdkPixbuf loader written for WebP, and adapted for libflif version 0.3.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <flif.h>

#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf.h>
#undef  GDK_PIXBUF_ENABLE_BACKEND

#if (_XOPEN_SOURCE >= 700 || _POSIX_C_SOURCE >= 200809L)
    #define HAVE_OPEN_MEMSTREAM
#endif

/* Progressive loader context */
typedef struct {
        GdkPixbufModuleSizeFunc size_func;
        GdkPixbufModuleUpdatedFunc update_func;
        GdkPixbufModulePreparedFunc prepare_func;

        guint32 w;
        guint32 h;
        gint bit_depth;
        gint nb_channels;

        gpointer user_data;
        GdkPixbuf *pixbuf;
        gboolean got_header;
        GError **error;

        FILE *increment_buffer;
#ifdef HAVE_OPEN_MEMSTREAM
        char *increment_buffer_ptr;
        size_t increment_buffer_size;
#endif

} FLIFContext;

/* Shared library entry point */
static GdkPixbuf *gdk_pixbuf__flif_image_load (FILE *f, GError **error) {

    size_t data_size;
    int status;
    gpointer data;

    // Get data size 
    fseek (f, 0, SEEK_END);
    data_size = ftell(f);
    fseek (f, 0, SEEK_SET);

    // Get data
    data = g_malloc (data_size);
    status = (fread (data, data_size, 1, f) == 1);
    if (!status) {
            g_set_error (error,
                GDK_PIXBUF_ERROR,
                GDK_PIXBUF_ERROR_FAILED,
                "Failed to read file");
            g_free(data);
            return NULL;
    }
   
    FLIF_DECODER* decoder = flif_create_decoder();

    status = flif_decoder_decode_memory(decoder, data, data_size);

    if (status <= 0) {
        g_set_error (error,
            GDK_PIXBUF_ERROR,
            GDK_PIXBUF_ERROR_FAILED,
            "Cannot create FLIF decoder.");
        flif_destroy_decoder(decoder);
        g_free (data);
        return NULL;
    }

    FLIF_IMAGE* image = flif_decoder_get_image(decoder, 0);

    if (!image) {
        g_set_error (error,
            GDK_PIXBUF_ERROR,
            GDK_PIXBUF_ERROR_FAILED,
            "No decoded image found.");
        flif_destroy_decoder(decoder);
        g_free (data);
        return NULL;
    }

    gint32 w = flif_image_get_width(image);
    gint32 h = flif_image_get_height(image);

    GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w, h);

    if (!pixbuf) {
            g_set_error (error,
                GDK_PIXBUF_ERROR,
                GDK_PIXBUF_ERROR_FAILED,
                "Failed to decode image");
            flif_destroy_decoder(decoder);
            g_free (data);
            return NULL;
    }

    // There may be padding at the end of the row for alignment purposes. Not necessarily w * 4
    gint32 rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
    guchar *rowpointer = pixels;
    for (uint32_t row = 0; row < (uint32_t)h; row++) {
        flif_image_read_row_RGBA8(image, row, rowpointer, w * 4);
        rowpointer += rowstride;
    }

    flif_destroy_decoder(decoder);
    g_free(data);

    return pixbuf;
}


static gpointer gdk_pixbuf__flif_image_begin_load (GdkPixbufModuleSizeFunc size_func, GdkPixbufModulePreparedFunc prepare_func,
                                   GdkPixbufModuleUpdatedFunc update_func, gpointer user_data, GError **error) {
    FLIFContext *context = g_new(FLIFContext, 1);
    context->size_func = size_func;
    context->prepare_func = prepare_func;
    context->update_func  = update_func;
    context->user_data = user_data;
    context->error = error;
    context->got_header = FALSE;

#ifdef HAVE_OPEN_MEMSTREAM
    context->increment_buffer = open_memstream(&context->increment_buffer_ptr, &context->increment_buffer_size);
#else
    context->increment_buffer = tmpfile();
#endif

    if (!context->increment_buffer){
        perror("Cannot create increment buffer.");
        g_free(context);
        return NULL;
    }

    return context;
}

static gboolean gdk_pixbuf__flif_image_stop_load (gpointer user_context, GError **error) {

    FLIFContext *context = (FLIFContext *) user_context;

    int status = fflush(context->increment_buffer);
    status |= fseek(context->increment_buffer, 0L, SEEK_SET);

    if (status != 0){
        perror("Cannot flush and rewind increment buffer.");
        fclose(context->increment_buffer);
#ifdef HAVE_OPEN_MEMSTREAM
        // we don't use g_free here because open_memstream uses the internal malloc from stdlib.h
        free(context->increment_buffer_ptr);
#endif
        g_free(context);
        return FALSE;
    }

    context->pixbuf = gdk_pixbuf__flif_image_load(context->increment_buffer, error);

    if (context->prepare_func) {
        (* context->prepare_func) (context->pixbuf, NULL, context->user_data);
    }

    if (context->update_func) {
        (* context->update_func) (context->pixbuf, 0, 0, context->w, context->h, context->user_data);
    }

    fclose(context->increment_buffer);

#ifdef HAVE_OPEN_MEMSTREAM
    // we don't use g_free here because open_memstream uses the internal malloc from stdlib.h
    free(context->increment_buffer_ptr);
#endif

    g_object_unref(context->pixbuf);
    g_free(context);

    return TRUE;
}

static gboolean gdk_pixbuf__flif_image_load_increment (gpointer user_context, const guchar *buf, guint size, GError **error) {
        
    FLIFContext *context = (FLIFContext *) user_context;

    int status = fwrite(buf, size, sizeof(guchar), context->increment_buffer);

    if (status != sizeof(guchar)){
        g_set_error(error,
            GDK_PIXBUF_ERROR,
            GDK_PIXBUF_ERROR_FAILED,
            "Can't write to increment buffer.");
        return FALSE;
    }

    status = fflush(context->increment_buffer);
    
    if (status != 0){
        g_set_error(error,
            GDK_PIXBUF_ERROR,
            GDK_PIXBUF_ERROR_FAILED,
            "Can't flush the increment buffer.");
        return FALSE;
    }


    if (!context->got_header) {
        
        FLIF_INFO* flif_info = flif_read_info_from_memory(buf, size);
        
        if (!flif_info){
                g_set_error (error,
                    GDK_PIXBUF_ERROR,
                    GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                    "Cannot read FLIF image header.");
                return FALSE;
        }

        context->w = flif_info_get_width(flif_info);
        context->h = flif_info_get_height(flif_info);
        context->nb_channels = flif_info_get_nb_channels(flif_info);
        context->bit_depth = flif_info_get_depth(flif_info);

        flif_destroy_info(flif_info);
        
        context->got_header = TRUE;

        if (context->size_func) {
            gint scaled_w = context->w;
            gint scaled_h = context->h;

            (* context->size_func) (&scaled_w, &scaled_h, context->user_data);
            if ((guint)scaled_w != context->w || (guint)scaled_h != context->h) {
                context->w = scaled_w;
                context->h = scaled_h;
            }
        }
        
    }
    return TRUE;
}

void fill_vtable (GdkPixbufModule *module) {
    module->load = gdk_pixbuf__flif_image_load;
    module->begin_load = gdk_pixbuf__flif_image_begin_load;
    module->stop_load = gdk_pixbuf__flif_image_stop_load;
    module->load_increment = gdk_pixbuf__flif_image_load_increment;
}

void fill_info (GdkPixbufFormat *info) {

    static GdkPixbufModulePattern signature[] = {
        { "FLIF", "    ", 100 },
        { NULL, NULL, 0 }
    };

    static gchar *mime_types[] = {
            "image/flif",
            "image/x-flif",
            NULL
    };

    static gchar *extensions[] = {
            "flif",
            "FLIF",
            NULL
    };

    info->name        = "flif";
    info->signature   = signature;
    info->description = "Free Lossless Image Format";
    info->mime_types  = mime_types;
    info->extensions  = extensions;
    info->flags       = GDK_PIXBUF_FORMAT_THREADSAFE;
    info->license     = "LGPL";
}
