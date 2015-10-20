#include <flif.h>
#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>

#pragma pack(push,1)
typedef struct RGBA
{
    uint8_t r,g,b,a;
} RGBA;
#pragma pack(pop)


int main(int argc, char **argv) {
    if (argc < 2 || argc > 2) {
        printf("Usage:  %s  image.flif\n",argv[0]);
        return 0;
    }
    FLIF_DECODER* d = flif_create_decoder();
    if (!d) return 1;
    flif_decoder_set_quality(d, 100);
    flif_decoder_set_scale(d, 1);
    printf("Decoding...\n");
    if (flif_decoder_decode_file(d, argv[1]) == 0) {
        printf("Error: decoding failed\n");
        return 1;
    }
    printf("Done.\n");

    FLIF_IMAGE* image = flif_decoder_get_image(d, 0);
    if (!image) { printf("Error: No decoded image found\n"); return 1; }
    uint32_t w = flif_image_get_width(image);
    uint32_t h = flif_image_get_height(image);

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("FLIF Viewer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, 0);
    if (!window) { printf("Error: Could not create window\n"); return 2; }
    SDL_Surface *canvas = SDL_GetWindowSurface(window);
    if (!canvas) { printf("Error: Could not create canvas\n"); return 2; }
    SDL_Surface *surf = SDL_CreateRGBSurface(0,w,h,32,0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);
    if (!surf) { printf("Error: Could not create surface\n"); return 2; }
    void* pp = surf->pixels;
    for (uint32_t r=0; r<h; r++) {
        flif_image_read_row_RGBA8(image, r, pp, w * sizeof(RGBA));
        pp += surf->pitch;
    }
    SDL_BlitSurface(surf,NULL,canvas,NULL);
    SDL_UpdateWindowSurface(window);
    SDL_Event e;
    while (1) {
        SDL_WaitEvent(&e);
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) {printf("Closed\n"); break;}
    }


    flif_destroy_decoder(d);

    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}
