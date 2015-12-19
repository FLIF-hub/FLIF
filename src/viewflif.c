/*
 Example application of FLIF decoder using libflif_dec
 Copyright (C) 2015  Jon Sneyers, LGPL v3+

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


// comment out to not decode progressively
#define PROGRESSIVE_DECODING

#pragma pack(push,1)
typedef struct RGBA { uint8_t r,g,b,a; } RGBA;
#pragma pack(pop)

FLIF_DECODER* d = NULL;
SDL_Window *window = NULL;
SDL_DisplayMode dm;
SDL_Renderer *renderer = NULL;
SDL_Texture* image_frame[256] = {};
SDL_Surface* decsurf = NULL;
SDL_Surface* bgsurf = NULL;
SDL_Surface* tmpsurf = NULL;
int quit = 0;
int frame = 0;
int nb_frames = 0;
int frame_delay[256] = {};

int drawing = 0;
int loading = 0;
int window_size_set = 0;
int framecount = 0;

void draw_image() {
    if (drawing) return;
    if (loading) return;
    if (!window) return;
    if (!renderer) { printf("Error: Could not get renderer\n"); return; }
    drawing=1;
    SDL_Rect ir = {};
    SDL_Rect wr = {};
    SDL_Rect tr = {};
    if (SDL_QueryTexture(image_frame[frame], NULL, NULL, &ir.w, &ir.h)) { printf("Error: Could not query texture\n"); return; };
    if (!window_size_set) {
      int w = ir.w, h = ir.h;
      int ww = (w > dm.w ? dm.w : w);
      int wh = (h > dm.h ? dm.h : h);
      if (ww > w * wh / h) ww = wh * w / h;
      else if (ww < w * wh / h) wh = ww * h / w;
      if (w > dm.w*8/10 && h > dm.h*8/10) { ww = ww*8/10; wh = wh*8/10; }
      SDL_SetWindowSize(window,ww,wh);
      SDL_SetWindowPosition(window,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED);
      if (w > dm.w*8/10 && h > dm.h*8/10) { SDL_MaximizeWindow(window); }
      window_size_set = 1;
    }

    SDL_GetWindowSize(window, &wr.w, &wr.h);
    framecount++;
    tr = wr;
    if (ir.w && ir.h) {
     // scale to fit window, but respect aspect ratio
     if (wr.w > ir.w * wr.h / ir.h) tr.w = wr.h * ir.w / ir.h;
     else if (wr.w < ir.w * wr.h / ir.h) tr.h = wr.w * ir.h / ir.w;
     tr.x = (wr.w - tr.w)/2;
     tr.y = (wr.h - tr.h)/2;

     // alternative below: only scale down, don't scale up
     /*
     // window smaller than image: scale down, but respect aspect ratio
     if (tr.w < ir.w) { tr.h=wr.w*ir.h/ir.w; tr.y=(wr.h-ir.h)/2;}
     if (tr.h < ir.h) { tr.w=wr.h*ir.w/ir.h; tr.x=(wr.w-ir.w)/2;}

     // window larger than image: center the image (don't scale up)
     if (tr.w > ir.w) { tr.x=(wr.w-ir.w)/2; tr.w=ir.w; }
     if (tr.h > ir.h) { tr.y=(wr.h-ir.h)/2; tr.h=ir.h; }
     */

     //printf("Rendering %ix%i frame on %ix%i (+%i,%i) target rectangle\n", ir.w, ir.h, tr.w, tr.h, tr.x, tr.y);
     SDL_RenderClear(renderer);
     if (SDL_RenderCopy(renderer, image_frame[frame], NULL, &tr)) printf("Error in SDL_RenderCopy\n");
     SDL_RenderPresent(renderer);
    }
    drawing=0;
}

int do_event(SDL_Event e) {
    if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) {printf("Closed\n"); quit=1; return 0;}
    if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED && renderer) { draw_image(); }
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_q) {printf("Quit\n"); quit=1; return 0;}
    return 1;
}

uint32_t progressive_render(int32_t quality, int64_t bytes_read) {
    loading = 1;
    while (drawing) {SDL_Delay(50);}
    printf("%lli bytes read, rendering at quality=%.2f%%\n",(unsigned long long int) bytes_read, 0.01*quality);
    FLIF_IMAGE* image = flif_decoder_get_image(d, 0);
    if (!image) { printf("Error: No decoded image found\n"); return 1; }
    uint32_t w = flif_image_get_width(image);
    uint32_t h = flif_image_get_height(image);
    if (!window) { printf("Error: Could not create window\n"); return 2; }
    char title[100];
    sprintf(title,"FLIF image decoded at %ix%i [read %lli bytes, quality=%.2f%%]",w,h,(unsigned long long int) bytes_read, 0.01*quality);
    SDL_SetWindowTitle(window,title);
    for (int f = 0; f < flif_decoder_num_images(d); f++) {
        FLIF_IMAGE* image = flif_decoder_get_image(d, f);
        if (!image) { printf("Error: No decoded image found\n"); return 1; }
        uint32_t w = flif_image_get_width(image);
        uint32_t h = flif_image_get_height(image);
        frame_delay[f] = flif_image_get_frame_delay(image);
        if (!decsurf) decsurf = SDL_CreateRGBSurface(0,w,h,32,0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);
        if (!decsurf) { printf("Error: Could not create surface\n"); return 1; }
        if (!tmpsurf) tmpsurf = SDL_CreateRGBSurface(0,w,h,32,0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);
        if (!tmpsurf) { printf("Error: Could not create surface\n"); return 1; }
        char* pp =(char*) tmpsurf->pixels;
        for (uint32_t r=0; r<h; r++) {
            flif_image_read_row_RGBA8(image, r, pp, w * sizeof(RGBA));
            pp += tmpsurf->pitch;
        }
        if (flif_image_get_nb_channels(image) > 3) {
          if (!bgsurf) {
            // Draw checkerboard background for image/animation with alpha channel
            bgsurf = SDL_CreateRGBSurface(0,w,h,32,0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);
            SDL_Rect sq; sq.w=20; sq.h=20;
            for (sq.y=0; sq.y<h; sq.y+=sq.h) for (sq.x=0; sq.x<w; sq.x+=sq.w)
              SDL_FillRect(bgsurf,&sq,((sq.y/sq.h + sq.x/sq.w)&1 ? 0xFF606060 : 0xFFA0A0A0));
          }
          SDL_BlitSurface(bgsurf,NULL,decsurf,NULL);
        }
        if (SDL_BlitSurface(tmpsurf,NULL,decsurf,NULL)) printf("Error: Could not blit decoded image\n");;
        if (!renderer) { printf("Error: Could not get renderer\n"); return 1; }
        if (image_frame[f]) SDL_DestroyTexture(image_frame[f]);
        image_frame[f] = SDL_CreateTextureFromSurface(renderer, decsurf);
        if (!image_frame[f]) { printf("Could not create texture!\n"); quit=1; return 0; }
        SDL_SetTextureBlendMode(image_frame[f],SDL_BLENDMODE_NONE);
        SDL_FreeSurface(tmpsurf); tmpsurf=NULL;
        SDL_FreeSurface(decsurf); decsurf=NULL;
    }
    SDL_FreeSurface(bgsurf); bgsurf=NULL;
    loading = 0;
    draw_image();
    int repeat = (nb_frames == 0 && quality < 10000);
    nb_frames = flif_decoder_num_images(d);
    // not sure why (todo: figure out what is happening here), but looks like the surfaces don't get properly written the first time
    if (repeat) progressive_render(quality,bytes_read);
    if (quit) return 0; // stop decoding
    return quality + 1000; // call me back when you have at least 10.00% better quality
}

static int decodeThread(void * arg) {
    char ** argv = (char **)arg;
    d = flif_create_decoder();
    if (!d) return 1;
    flif_decoder_set_quality(d, 100);   // this is the default, so can be omitted
    flif_decoder_set_scale(d, 1);       // this is the default, so can be omitted
//    flif_decoder_set_resize(d, dm.w, dm.h); // resize (subsample) to be smaller than the desktop resolution
    flif_decoder_set_resize(d, dm.w*2, dm.h*2); // resize (subsample), but decode enough for a good quality scaled-down image
#ifdef PROGRESSIVE_DECODING
    flif_decoder_set_callback(d, &(progressive_render));
    flif_decoder_set_first_callback_quality(d, 500);   // do the first callback when at least 5.00% quality has been decoded
#endif
    if (flif_decoder_decode_file(d, argv[1]) == 0) {
        printf("Error: decoding failed\n");
        quit = 1;
        flif_destroy_decoder(d);
        return 1;
    }
#ifndef PROGRESSIVE_DECODING
    progressive_render(10000,-1);
#endif
    flif_destroy_decoder(d);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 2) {
        printf("Usage:  %s  image.flif\n",argv[0]);
        return 0;
    }

    SDL_Init(SDL_INIT_VIDEO);
    SDL_EventState(SDL_MOUSEMOTION,SDL_IGNORE);
    window = SDL_CreateWindow("FLIF Viewer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 200, 200, SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawColor(renderer, 127, 127, 127, 255); // background color (in case aspect ratio of window doesn't match image)
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    if (SDL_GetWindowDisplayMode(window,&dm)) { printf("Error: SDL_GetWindowDisplayMode\n"); return 1; }
#ifdef PROGRESSIVE_DECODING
    printf("Decoding progressively...\n");
    SDL_Thread *decode_thread = SDL_CreateThread(decodeThread,"Decode_FLIF",argv);
    if (!decode_thread) {
        printf("Error: failed to create decode thread\n");
        return 1;
    }
#else
    printf("Decoding entire image...\n");
    decodeThread(argv);
#endif
    SDL_Event e;
    int result = 0;
    unsigned int current_time;
    unsigned int begin=SDL_GetTicks();
    while (!quit) {
        if (nb_frames > 1) {
            current_time = SDL_GetTicks();
            draw_image();
            int time_passed = SDL_GetTicks()-current_time;
            int time_to_wait = frame_delay[frame] - time_passed;
            if (time_to_wait>0) SDL_Delay(time_to_wait);  // todo: if the animation has extremely long frame delays, this makes the viewer unresponsive
            frame++;
            frame %= nb_frames;
        } else {
            SDL_Delay(200); // if it's not an animation, check event queue 5 times per second
        }
        while (SDL_PollEvent(&e)) do_event(e);
    }
    if (nb_frames > 1) printf("Rendered %i frames in %.2f seconds, %.4f frames per second\n", framecount, 0.001*(SDL_GetTicks()-begin), 1000.0*framecount/(SDL_GetTicks()-begin));
#ifdef PROGRESSIVE_DECODING
    while(flif_abort_decoder(d)) SDL_Delay(100);
    SDL_WaitThread(decode_thread, &result);
#endif
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
