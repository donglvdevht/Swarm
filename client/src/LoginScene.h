#pragma once
#include <array>

#include "Scene.h"
#include "Network.h"

class LoginScene : public Scene
{
public:
    LoginScene(Network& net);
    void handleEvent(const SDL_Event& e) override;
    void update(float deltaTime) override;
    void render(const SDLState& state, float deltaTime) override;
private:
    enum class FormState { Editing, Submitting };

    Network& network_;

    FormState formState_ = FormState::Editing;
    std::array<char, MAX_NAME_LEN + 1> username_{};
    std::array<char, MAX_NAME_LEN + 1> password_{};
    bool validationError_ = false;
    void submit();
};
