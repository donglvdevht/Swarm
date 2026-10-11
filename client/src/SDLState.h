#pragma once
#include <SDL3/SDL.h>

struct SDLState
{
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    bool imguiContextInitialized = false;
    bool imguiPlatformInitialized = false;
    bool imguiRendererInitialized = false;
    int logicalWidth   = 1920;
    int logicalHeight  = 1080;
};
