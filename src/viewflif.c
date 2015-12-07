/*
 Example application of FLIF decoder - Free Lossless Image Format
 Copyright (C) 2010-2015  Jon Sneyers & Pieter Wuille, LGPL v3+

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <flif_dec.h>
#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>

#pragma pack(push,1)
typedef struct RGBA
{
    uint8_t r,g,b,a;
} RGBA;
#pragma pack(pop)

FLIF_DECODER* d = NULL;
SDL_Window *window = NULL;
SDL_Surface* surf[256] = {};
SDL_Surface* bgsurf = NULL;
SDL_Surface* tmpsurf = NULL;
int quit = 0;
int frame = 0;
int frame_delay[256] = {};

int do_event(SDL_Event e) {
       if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) {printf("Closed\n"); quit=1; return 0;}
       if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_q) {printf("Quit\n"); quit=1; return 0;}
       return 1;
}

void draw_image() {
    if (!window) return;
    SDL_Surface *canvas = SDL_GetWindowSurface(window);
    if (!canvas) { printf("Error: Could not get canvas\n"); return; }
    SDL_BlitSurface(surf[frame],NULL,canvas,NULL);
    SDL_UpdateWindowSurface(window);
}

uint32_t progressive_render(int32_t quality, int64_t bytes_read) {
    printf("%lli bytes read, rendering at quality=%.2f%%\n",bytes_read, 0.01*quality);
    FLIF_IMAGE* image = flif_decoder_get_image(d, 0);
    if (!image) { printf("Error: No decoded image found\n"); return 1; }
    uint32_t w = flif_image_get_width(image);
    uint32_t h = flif_image_get_height(image);
    if (!window) { printf("Error: Could not create window\n"); return 2; }
    SDL_SetWindowSize(window,w,h);
    char title[100];
    sprintf(title,"%ix%i FLIF image [read %lli bytes, quality=%.2f%%]",w,h,bytes_read, 0.01*quality);
    SDL_SetWindowTitle(window,title);
    for (int f = 0; f< flif_decoder_num_images(d); f++) {
        FLIF_IMAGE* image = flif_decoder_get_image(d, f);
        if (!image) { printf("Error: No decoded image found\n"); return 1; }
        uint32_t w = flif_image_get_width(image);
        uint32_t h = flif_image_get_height(image);
        frame_delay[f] = flif_image_get_frame_delay(image);
        if (!surf[f]) surf[f] = SDL_CreateRGBSurface(0,w,h,32,0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);
        if (!surf[f]) { printf("Error: Could not create surface\n"); return 1; }
        if (!tmpsurf) tmpsurf = SDL_CreateRGBSurface(0,w,h,32,0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);
        if (!tmpsurf) { printf("Error: Could not create surface\n"); return 1; }
        char* pp =(char*) tmpsurf->pixels;
        for (uint32_t r=0; r<h; r++) {
            flif_image_read_row_RGBA8(image, r, pp, w * sizeof(RGBA));
            pp += tmpsurf->pitch;
        }
        if (flif_image_get_nb_channels(image) > 3) {
          if (!bgsurf) bgsurf = SDL_CreateRGBSurface(0,w,h,32,0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);
          // SDL_FillRect(canvas,NULL,0);
          // Draw checkerboard background for image with alpha channel
          SDL_Rect sq; sq.w=20; sq.h=20;
          for (sq.y=0; sq.y<h; sq.y+=sq.h) for (sq.x=0; sq.x<w; sq.x+=sq.w)
              SDL_FillRect(bgsurf,&sq,((sq.y/sq.h + sq.x/sq.w)&1 ? 0xFF606060 : 0xFFA0A0A0));
          SDL_BlitSurface(bgsurf,NULL,surf[f],NULL);
        }
        SDL_BlitSurface(tmpsurf,NULL,surf[f],NULL);
    }

    draw_image();
//    SDL_Delay(1000);
    if (quit) return 0; // stop decoding
    return quality + 1000; // call me back when you have at least 10.00% better quality
}

static int decodeThread(void * arg) {
    char ** argv = (char **)arg;
    if (flif_decoder_decode_file(d, argv[1]) == 0) {
        printf("Error: decoding failed\n");
        quit = 1;
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 2) {
        printf("Usage:  %s  image.flif\n",argv[0]);
        return 0;
    }
    d = flif_create_decoder();
    if (!d) return 1;

    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("FLIF Viewer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 100, 100, 0);
    flif_decoder_set_quality(d, 100);   // this is the default
    flif_decoder_set_scale(d, 1);       // this is the default
    flif_decoder_set_callback(d, &(progressive_render));
    flif_decoder_set_first_callback_quality(d, 500);   // do the first callback when at least 5.00% quality has been decoded
    printf("Decoding...\n");
    SDL_Thread *decode_thread = SDL_CreateThread(decodeThread,"Decode_FLIF",argv);
    if (!decode_thread) {
        printf("Error: failed to create decode thread\n");
        return 1;
    }
    SDL_Event e;
    int animation = 1;
    int result = 0;
    while (!quit) {
        if (window && animation) {
            draw_image();
            if (flif_decoder_num_images(d) > 1) {
              SDL_Delay(frame_delay[frame]);
              frame++;
              frame %= flif_decoder_num_images(d);
            } else animation = 0;
        }
        while (SDL_PollEvent(&e)) {
            if (!do_event(e)) break;
        }
    }
    SDL_WaitThread(decode_thread, &result);
    flif_destroy_decoder(d);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
