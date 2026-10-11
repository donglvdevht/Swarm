#pragma once
#include <SDL3/SDL.h>
#include <map>

class InputHandler {
public:
    enum class Action {
        MoveLeft, MoveRight, MoveUp, MoveDown,
        Attack, Run, Jump, Count
    };

    // Handle SDL_Event (SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP)
    void handleEvent(const SDL_Event& event);
    
    bool isPressed(Action action);
    
    bool isHeld(Action action);

    // Call at end of frame to update previous state
    void update();
private:
    // State tracking: [current frame] and [previous frame]
    bool currentState[(int)Action::Count] = {};
    bool previousState[(int)Action::Count] = {};
    
    // Key to Action mapping
    std::map<SDL_Keycode, Action> keyMap = {
        {SDLK_A,        Action::MoveLeft},
        {SDLK_LEFT,     Action::MoveLeft},
        {SDLK_D,        Action::MoveRight},
        {SDLK_RIGHT,    Action::MoveRight},
        {SDLK_S,        Action::MoveDown},
        {SDLK_DOWN,     Action::MoveDown},
        {SDLK_W,        Action::MoveUp},
        {SDLK_UP,       Action::MoveUp},
        {SDLK_J,        Action::Attack},
        {SDLK_LSHIFT,   Action::Run},
        {SDLK_RSHIFT,   Action::Run},
        {SDLK_SPACE,    Action::Jump},
    };
};