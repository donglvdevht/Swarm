#pragma once
#include <SDL3/SDL.h>

struct SDLState
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    int logicalWidth   = 1920;
    int logicalHeight  = 1080;
};
