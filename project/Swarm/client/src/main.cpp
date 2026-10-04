#include <cstdio>
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL_stdinc.h>
#include <array>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include <deque>
#include <memory>

#include "AnimationData.h"
#include "Animation.h"
#include "GameObject.h"
#include "Resources.h"
#include "SDLState.h"
#include "AnimationTypes.h"
#include "InputHandler.h"
#include "Network.h"
#include "GameScene.h"
#include "LoginScene.h"

#ifndef SWARM_ASSET_DIR
#define SWARM_ASSET_DIR "."
#endif

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

constexpr uint64_t TARGET_FRAME_TIME_MS = 8;

void cleanup(SDLState &state);
bool initialize(SDLState &state);

int main() {
    // --- Create the window ---
    SDLState state;
    if (initialize(state))
        return -1;

    // --- Create the metwork ---
    Network network;
    network.start("127.0.0.1", 9000);

    Resources res;
    res.load(state);

    std::unique_ptr<Scene> scene = std::make_unique<LoginScene>(network);
    uint64_t prevTime = SDL_GetTicks();

    // --- Đếm FPS ---
    uint64_t fpsTimerMs = SDL_GetTicks();
    int frameCounter = 0;
    int currentFps = 0;

    // --- Start the game loop ---
    bool running = true;
    while (running)
    {
        uint64_t nowTime = SDL_GetTicks();
        float deltaTime = (nowTime - prevTime) / 1000.0f;
        deltaTime = std::min(deltaTime, 0.1f);

        frameCounter++;
        if (nowTime - fpsTimerMs >= 1000)
        {
            currentFps = frameCounter;
            frameCounter = 0;
            fpsTimerMs = nowTime;
        }

        SDL_Event event{0};
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT) 
            {
                running = false; 
                break;
            }
            scene->handleEvent(event);
        }
        scene->update(deltaTime);

        // --- Vẽ ---
        SDL_SetRenderDrawColor(state.renderer, 20, 10, 30, 255);
        SDL_RenderClear(state.renderer);

        scene->render(state, deltaTime);

        // --- Hiển thị FPS ở góc trên-trái ---
        SDL_SetRenderDrawColor(state.renderer, 255, 255, 0, 255);
        SDL_SetRenderScale(state.renderer, 2.0f, 2.0f);
        SDL_RenderDebugTextFormat(state.renderer, 5.0f, 5.0f, "FPS: %d", currentFps);
        SDL_SetRenderScale(state.renderer, 1.0f, 1.0f);

        SDL_RenderPresent(state.renderer);

        scene->endFrame();

        switch (scene->requestedScene()) {      // chuyển scene
            case SceneId::Quit: running = false; break;
            case SceneId::Game: scene = std::make_unique<GameScene>(state, res, network); break;
            default: break;
        }

        const uint64_t frameTime = SDL_GetTicks() - nowTime;
        if (frameTime < TARGET_FRAME_TIME_MS) {
            SDL_Delay(static_cast<uint32_t>(TARGET_FRAME_TIME_MS - frameTime));
        }
        prevTime = nowTime;
    }

    scene.reset();
    res.unload();
    cleanup(state);
    return 0;
}

void cleanup(SDLState &state)
{
    SDL_DestroyRenderer(state.renderer);
    SDL_DestroyWindow(state.window);
    SDL_Quit();
}

bool initialize(SDLState &state)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error initializing SDL3", nullptr);
        return 1;
    }
    state.window = SDL_CreateWindow("Swarm", state.logicalWidth, state.logicalHeight, 0);
    if (!state.window) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating window", nullptr);
        cleanup(state);
        return 1;
    }

    state.renderer = SDL_CreateRenderer(state.window, nullptr);
    if (!state.renderer)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating renderer", nullptr);
        cleanup(state);
        return 1;
    }

    SDL_SetRenderLogicalPresentation(state.renderer, state.logicalWidth, state.logicalHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    return 0;
}
