/*
 Example application of FLIF decoder using libflif_dec
 Copyright (C) 2015  Jon Sneyers

 License: Creative Commons CC0 1.0 Universal (Public Domain)
 https://creativecommons.org/publicdomain/zero/1.0/legalcode
*/


#include <flif_dec.h>
#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>
#include <time.h>
#include <stdbool.h>

// SDL2 for Visual Studio C++ 2015

#if _MSC_VER >= 1900
FILE _iob[] = {*stdin, *stdout, *stderr};
extern "C" FILE * __cdecl __iob_func(void)
{
    return _iob;
}
void HackToReferencePrintEtc()
{
    fprintf(stderr, "");
}
#endif

// comment out to not decode progressively
#define PROGRESSIVE_DECODING

#pragma pack(push,1)
typedef struct RGBA { uint8_t r,g,b,a; } RGBA;
#pragma pack(pop)

FLIF_DECODER* d = NULL;
SDL_Window* window = NULL;
SDL_DisplayMode dm;
SDL_DisplayMode ddm;
SDL_Renderer* renderer = NULL;
SDL_Texture** image_frame = NULL;
SDL_Surface* decsurf = NULL;
SDL_Surface* bgsurf = NULL;
SDL_Surface* tmpsurf = NULL;
volatile int quit = 0;
int frame = 0;
volatile int nb_frames = 0;
int* frame_delay = NULL;

SDL_mutex *volatile mutex;

int window_size_set = 0;
int framecount = 0;

// Renders the image or current animation frame (assuming it is available as a SDL_Texture)
void draw_image() {
    if (!window) return;
    if (SDL_LockMutex(mutex) == 0) {
      if (!renderer) { printf("Error: Could not get renderer\n"); return; }
      SDL_Rect ir = {}; // image rectangle (source texture)
      SDL_Rect wr = {}; // window rectangle
      SDL_Rect tr = {}; // target rectangle
      if (SDL_QueryTexture(image_frame[frame], NULL, NULL, &ir.w, &ir.h)) { printf("Error: Could not query texture\n"); return; };
      if (!ir.w || !ir.h) { printf("Error: Empty texture ?\n"); return; };
      framecount++;
      SDL_GetWindowSize(window, &wr.w, &wr.h);
      tr = wr;
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

      // if target has an offset, make sure there is a background
      if (tr.x || tr.y) SDL_RenderClear(renderer);
      // blit the frame into the target area
      SDL_RenderCopy(renderer, image_frame[frame], NULL, &tr);
      // flip the framebuffer
      SDL_RenderPresent(renderer);
      SDL_UnlockMutex(mutex);
    } else {
      fprintf(stderr, "Couldn't lock mutex\n");
    }
}

int do_event(SDL_Event e) {
    if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) {printf("Closed\n"); quit=1; return 0;}
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_q) {printf("Quit\n"); quit=1; return 0;}
    // refresh the window if its size changes
    if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED && renderer) { draw_image(); }
    return 1;
}


// returns true on success
bool updateTextures(uint32_t quality, int64_t bytes_read) {
    printf("%lli bytes read, rendering at quality=%.2f%%\n",(long long int) bytes_read, 0.01*quality);

    FLIF_IMAGE* image = flif_decoder_get_image(d, 0);
    if (!image) { printf("Error: No decoded image found\n"); return false; }
    uint32_t w = flif_image_get_width(image);
    uint32_t h = flif_image_get_height(image);

    // set the window title and size
    if (!window) { printf("Error: Could not create window\n"); return false; }
    char title[100];
    sprintf(title,"FLIF image decoded at %ix%i [read %lli bytes, quality=%.2f%%]",w,h,(long long int) bytes_read, 0.01*quality);
    SDL_SetWindowTitle(window,title);
    if (!window_size_set) {
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

    // allocate enough room for the texture pointers and frame delays
    if (!image_frame) image_frame = (SDL_Texture**) calloc(flif_decoder_num_images(d), sizeof(FLIF_IMAGE*));
    if (!frame_delay) frame_delay = (int*) calloc(flif_decoder_num_images(d), sizeof(int));

    // produce one SDL_Texture per frame
    for (int f = 0; f < flif_decoder_num_images(d); f++) {
        if (quit) {
          return 0;
        }
        FLIF_IMAGE* image = flif_decoder_get_image(d, f);
        if (!image) { printf("Error: No decoded image found\n"); return false; }
        frame_delay[f] = flif_image_get_frame_delay(image);
        // Copy the decoded pixels to a temporary surface
        if (!tmpsurf) tmpsurf = SDL_CreateRGBSurface(0,w,h,32,0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);
        if (!tmpsurf) { printf("Error: Could not create surface\n"); return false; }
        char* pp =(char*) tmpsurf->pixels;
        for (uint32_t r=0; r<h; r++) {
            flif_image_read_row_RGBA8(image, r, pp, w * sizeof(RGBA));
            pp += tmpsurf->pitch;
        }
        // Draw checkerboard background for image/animation with alpha channel
        if (flif_image_get_nb_channels(image) > 3) {
          if (!bgsurf) bgsurf = SDL_CreateRGBSurface(0,w,h,32,0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);
          if (!bgsurf) { printf("Error: Could not create surface\n"); return false; }
          SDL_Rect sq; sq.w=20; sq.h=20;
          for (sq.y=0; sq.y<h; sq.y+=sq.h) for (sq.x=0; sq.x<w; sq.x+=sq.w)
              SDL_FillRect(bgsurf,&sq,(((sq.y/sq.h + sq.x/sq.w)&1) ? 0xFF606060 : 0xFFA0A0A0));
          // Alpha-blend decoded frame on top of checkerboard background
          SDL_BlitSurface(tmpsurf,NULL,bgsurf,NULL);
          SDL_FreeSurface(tmpsurf); tmpsurf = bgsurf; bgsurf = NULL;
        }
        if (!renderer) { printf("Error: Could not get renderer\n"); return false; }
        if (image_frame[f]) SDL_DestroyTexture(image_frame[f]);
        // Convert the surface to a texture (for accelerated blitting)
        image_frame[f] = SDL_CreateTextureFromSurface(renderer, tmpsurf);
        if (!image_frame[f]) { printf("Could not create texture!\n"); quit=1; return 1; }
        SDL_SetTextureBlendMode(image_frame[f],SDL_BLENDMODE_NONE);
    }
    SDL_FreeSurface(tmpsurf); tmpsurf=NULL;

    return true;
}

#ifdef PROGRESSIVE_DECODING
const double preview_interval= .6;

clock_t last_preview_time = 0;

// Callback function: converts (partially) decoded image/animation to a/several SDL_Texture(s),
//                    resizes the viewer window if needed, and calls draw_image()
// Input arguments are: quality (0..10000), current position in the .flif file
// Output is the desired minimal quality before doing the next callback
uint32_t progressive_render(uint32_t quality, int64_t bytes_read, uint8_t decode_over, void *user_data, void *context) {
    if (SDL_LockMutex(mutex) == 0) {
      clock_t now = clock();
      double timeElapsed = ((double)(now - last_preview_time)) / CLOCKS_PER_SEC;
      if (quality != 10000 && (!decode_over) && timeElapsed< preview_interval) {
        SDL_UnlockMutex(mutex);
        return quality + 1000;
      }

      // For benchmarking
      // clock_t finalTime = clock();
      // if (quality == 10000) printf("Total time: %.2lf\n", ((double)finalTime ) / CLOCKS_PER_SEC);

      flif_decoder_generate_preview(context);

      bool success = updateTextures(quality, bytes_read);

      last_preview_time = clock();

      SDL_UnlockMutex(mutex);

      if (!success || quit) {
        return 0; // stop decoding
      } else {
        // setting nb_frames to a value > 1 will make sure the main thread keeps calling draw_image()
        nb_frames = flif_decoder_num_images(d);
        draw_image();
      }

      return quality + 1000; // call me back when you have at least 10.00% better quality
    } else {
      fprintf(stderr, "Couldn't lock mutex\n");
      return 0;
    }

}
#endif

// When decoding progressively, this is a separate thread (so a partially loaded animation keeps playing while decoding more detail)
static int decodeThread(void * arg) {
    char ** argv = (char **)arg;
    d = flif_create_decoder();
    if (!d) return 1;
    // set the quality to 100% (a lower value will decode a lower-quality preview)
    flif_decoder_set_quality(d, 100);             // this is the default, so can be omitted
    // set the scale-down factor to 1 (a higher value will decode a downsampled preview)
    flif_decoder_set_scale(d, 1);                 // this is the default, so can be omitted
    // set the maximum size to twice the screen resolution; if an image is larger, a downsampled preview will be decoded
    flif_decoder_set_resize(d, ddm.w*2, ddm.h*2);   // the default is to not have a maximum size

    // alternatively, set the decode width to exactly the screen width (the height will be set to respect aspect ratio)
    // flif_decoder_set_fit(d, dm.w, 0);   // the default is to not have a maximum size
#ifdef PROGRESSIVE_DECODING
    // set the callback function to render the partial (and final) decoded images
    flif_decoder_set_callback(d, &(progressive_render), NULL);  // the default is "no callback"; decode completely until quality/scale/size target is reached
    // do the first callback when at least 5.00% quality has been decoded
    flif_decoder_set_first_callback_quality(d, 500);      // the default is to callback almost immediately
#endif
    if (!flif_decoder_decode_file(d, argv[1])) {
        printf("Error: decoding failed\n");
        quit = 1;
        flif_destroy_decoder(d);
        return 1;
    }
#ifndef PROGRESSIVE_DECODING
    // no callback was set, so we manually call our callback function to render the final image/frames
    updateTextures(10000,-1);
#endif
    flif_destroy_decoder(d);
    d = NULL;
    return 0;
}

bool abort_decode() {
  if (SDL_LockMutex(mutex) == 0) {
    int retValue = flif_abort_decoder(d);
    SDL_UnlockMutex(mutex);
    return retValue;
  } else {
    return false;
  }
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 2) {
        printf("Usage:  %s  image.flif\n",argv[0]);
        return 0;
    }

    mutex = SDL_CreateMutex();
    if (!mutex) {
      fprintf(stderr, "Couldn't create mutex\n");
      return 1;
    }

#ifdef PROGRESSIVE_DECODING
    last_preview_time = (-2*preview_interval* CLOCKS_PER_SEC);
#endif

    SDL_Init(SDL_INIT_VIDEO);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

    SDL_EventState(SDL_MOUSEMOTION,SDL_IGNORE);
    window = SDL_CreateWindow("FLIF Viewer -- Loading...", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 200, 200, SDL_WINDOW_RESIZABLE);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawColor(renderer, 127, 127, 127, 255); // background color (in case aspect ratio of window doesn't match image)
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    int displayIndex = SDL_GetWindowDisplayIndex(window);
    if (SDL_GetDesktopDisplayMode(displayIndex,&ddm)) { printf("Error: SDL_GetWindowDisplayMode\n"); return 1; }
    if (SDL_GetWindowDisplayMode(window,&dm)) { printf("Error: SDL_GetWindowDisplayMode\n"); return 1; }
    int result = 0;
#ifdef PROGRESSIVE_DECODING
    printf("Decoding progressively...\n");
    SDL_Thread *decode_thread = SDL_CreateThread(decodeThread,"Decode_FLIF",argv);
    if (!decode_thread) {
        printf("Error: failed to create decode thread\n");
        return 1;
    }
#else
    printf("Decoding entire image...\n");
    result = decodeThread(argv);
#endif
    SDL_Event e;
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
    // make sure the decoding gets properly aborted (in case it was not done yet)
    while(d != NULL && abort_decode()) SDL_Delay(100);
    SDL_WaitThread(decode_thread, &result);
#endif
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
