#pragma once
#include <SDL3/SDL.h>
#include "SDLState.h"

enum class SceneId { None, Login, Game, Quit};

class Scene{
public:
    virtual ~Scene() = default;
    virtual void handleEvent(const SDL_Event& e) = 0;
    virtual void update(float deltaTime) = 0;
    virtual void render(const SDLState& state, float deltaTime) = 0;
    virtual void endFrame() {}
    SceneId requestedScene() const { return SceneNext_; }
protected:
    SceneId SceneNext_ = SceneId::None;
};