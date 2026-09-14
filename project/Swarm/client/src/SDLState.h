#pragma once
#include <SDL3/SDL.h>

struct SDLState
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    int logW = 1440;
    int logH = 720;
};
