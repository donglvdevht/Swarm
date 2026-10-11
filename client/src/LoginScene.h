#pragma once
#include <string>

#include "Scene.h"
#include "Network.h"

struct TextField
{
    SDL_FRect   rect{};              // toạ độ LOGIC
    std::string label;
    std::string text;
    bool        isPassword = false;
    bool        isFilled = false;
    size_t      maxLen = MAX_NAME_LEN;
};

class LoginScene : public Scene
{
public:
    LoginScene(Network& net);
    ~LoginScene();
    void handleEvent(const SDL_Event& e) override;
    void update(float deltaTime) override;
    void render(const SDLState& state, float deltaTime) override;
    void setFocusField(int index) {focusedField_ = index;};
private:
    enum class FormState {Editting, Submitting};

    Network& network_;

    FormState formState_ = FormState::Editting;
    std::vector<TextField> textField_;
    int focusedField_ = -1;
    void focusField(int index);
    void submit();
};
