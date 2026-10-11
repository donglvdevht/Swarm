#include "InputHandler.h"
#include <vector>

bool InputHandler::isHeld(Action action)
{
    return currentState[(int)action];
}

bool InputHandler::isPressed(Action action)
{
    return currentState[(int)action] && !previousState[(int)action];  // current=true AND previous=false
}

void InputHandler::update()
{
    // Copy current state to previous for next frame
    for (int i = 0; i < (int)Action::Count; ++i)
    {
        previousState[i] = currentState[i];
    }
}

void InputHandler::handleEvent (const SDL_Event& event)
{
    if (event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP) {
        return;
    }

    auto it = keyMap.find(event.key.key);
    if (it == keyMap.end()) {
        return;
    }

    const Action action = it->second;
    const bool pressed = (event.type == SDL_EVENT_KEY_DOWN);
    currentState[(int)action] = pressed;
}
