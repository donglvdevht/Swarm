#include "LoginScene.h"

#include <algorithm>
#include <string_view>

LoginScene::LoginScene(Network& net) : network_(net)
{
    textField_.resize(2);

    textField_[0] = TextField{
        {640, 420, 640, 72},
        "Account",
        "",
        false,
        false, 
        MAX_NAME_LEN
    };

    textField_[1] = TextField{
        {640, 530, 640, 72},
        "Password",
        "",
        true,
        false,
        MAX_NAME_LEN
    };
}

LoginScene::~LoginScene()
{
    if (SDL_Window* window = SDL_GetKeyboardFocus())
        SDL_StopTextInput(window);
}

void LoginScene::handleEvent(const SDL_Event& e)
{
    if (formState_ == FormState::Submitting)
        return;

    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        e.button.button == SDL_BUTTON_LEFT)
    {
        float x = e.button.x;
        float y = e.button.y;
        SDL_Window* window = SDL_GetKeyboardFocus();

        for (size_t i = 0; i < textField_.size(); ++i)
        {
            const SDL_FRect& rect = textField_[i].rect;
            if (x >= rect.x && x <= rect.x + rect.w &&
                y >= rect.y && y <= rect.y + rect.h)
            {
                focusField(static_cast<int>(i));
                return;
            }
        }
        focusField(-1);
        return;
    }

    if (e.type == SDL_EVENT_TEXT_INPUT && focusedField_ >= 0)
    {
        TextField& field = textField_[focusedField_];
        const std::string_view text(e.text.text);
        size_t accepted = 0;
        while (accepted < text.size())
        {
            const unsigned char lead = static_cast<unsigned char>(text[accepted]);
            const size_t charBytes = lead < 0x80 ? 1 :
                (lead & 0xE0) == 0xC0 ? 2 :
                (lead & 0xF0) == 0xE0 ? 3 :
                (lead & 0xF8) == 0xF0 ? 4 : 1;
            if (accepted + charBytes > text.size() ||
                field.text.size() + accepted + charBytes > field.maxLen)
                break;
            accepted += charBytes;
        }
        field.text.append(text.substr(0, accepted));
        return;
    }

    if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat)
        return;

    switch (e.key.key)
    {
        case SDLK_TAB:
            focusField((focusedField_ + 1) % static_cast<int>(textField_.size()));
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (focusedField_ >= 1)
            {
                submit();
                break;
            }

            if (focusedField_ < 1) focusField((focusedField_ + 1) % static_cast<int>(textField_.size()));
            break;
        case SDLK_BACKSPACE:
            if (focusedField_ >= 0)
            {
                std::string& text = textField_[focusedField_].text;
                if (!text.empty())
                {
                    size_t start = text.size() - 1;
                    while (start > 0 &&
                           (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80)
                        --start;
                    text.erase(start);
                }
                else if (focusedField_ > 0)
                {
                    focusField((focusedField_ - 1) % static_cast<int>(textField_.size()));
                }
            }
            break;
        case SDLK_ESCAPE:
            focusField(-1);
            break;
        default:
            break;
    }
}

void LoginScene::focusField(int index)
{
    focusedField_ = index;
    for (size_t i = 0; i < textField_.size(); ++i)
        textField_[i].isFilled = static_cast<int>(i) == focusedField_;

    SDL_Window* window = SDL_GetKeyboardFocus();
    if (!window)
        return;
    if (focusedField_ >= 0)
    {
        if (!SDL_StartTextInput(window))
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not start text input: %s", SDL_GetError());
    }
    else
    {
        SDL_StopTextInput(window);
    }
}

void LoginScene::submit()
{
    if (textField_[0].text.empty() || textField_[1].text.empty())
        return;

    network_.beginLogin(textField_[0].text, textField_[1].text);
    formState_ = FormState::Submitting;
    if (SDL_Window* window = SDL_GetKeyboardFocus())
        SDL_StopTextInput(window);
}

void LoginScene::update(float deltaTime)
{
    if (formState_ != FormState::Submitting)
        return;

    switch (network_.getConnState())
    {
    case ConnState::Connected:
        SceneNext_ = SceneId::Game;
        break;
    case ConnState::Failed:
        formState_ = FormState::Editting;
        switch (network_.getLastStatus())
        {
            case static_cast<uint8_t>(LoginStatus::WRONG_PASSWORD):
                textField_[1].text.clear();
                focusField(1);
                break;
            case static_cast<uint8_t>(LoginStatus::NAME_TAKEN):
                textField_[0].text.clear();
                textField_[1].text.clear();
                focusField(0);
                break;
            default:
                focusField(focusedField_ >= 0 ? focusedField_ : 0);
                break;
        }
        break;
    default:
        break;
    }
}

void LoginScene::render(const SDLState& state, float deltaTime)
{
    // --- Vẽ ô ---
    // Nền
    for (const TextField& txtFld : textField_)
    {
        if (txtFld.isFilled)
            SDL_SetRenderDrawColor(state.renderer, 100, 180, 255, 255);
        else
            SDL_SetRenderDrawColor(state.renderer, 60, 60, 70, 255);


        SDL_RenderFillRect(state.renderer, &txtFld.rect);

        SDL_SetRenderDrawColor(state.renderer, 200, 200, 200, 255);
        SDL_RenderRect(state.renderer, &txtFld.rect);
    }

    SDL_SetRenderScale(state.renderer, 2.0f, 2.0f);
    for (const TextField& txtFld : textField_)
    {
        SDL_SetRenderDrawColor(state.renderer, 230, 230, 230, 255);
        // Hiển thị label
        SDL_RenderDebugText(state.renderer, (txtFld.rect.x + 10.0f) / 2.0f,
                            (txtFld.rect.y + 8.0f) / 2.0f, txtFld.label.c_str());

        std::string displayText;
        if (txtFld.isPassword)
        {
            for (unsigned char byte : txtFld.text)
            {
                if ((byte & 0xC0) != 0x80)
                    displayText.push_back('*');
            }
        }
        else
        {
            displayText = txtFld.text;
        }

        if (displayText.empty())
            displayText = txtFld.isPassword ? "Enter password" : "Enter account";

        SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(state.renderer, (txtFld.rect.x + 10.0f) / 2.0f,
                            (txtFld.rect.y + 40.0f) / 2.0f, displayText.c_str());
    }
    SDL_SetRenderScale(state.renderer, 1.0f, 1.0f);

    if (network_.getConnState() == ConnState::Failed)
    {
        const char* msg = "Dang nhap that bai";
        switch (network_.getLastStatus())
        {
            case static_cast<uint8_t>(LoginStatus::WRONG_PASSWORD): msg = "Mat khau khong dung"; break;
            case static_cast<uint8_t>(LoginStatus::NAME_TAKEN):     msg = "Tai khoan khong ton tai"; break;
            case static_cast<uint8_t>(LoginStatus::SERVER_FULL):    msg = "May chu da day"; break;
            case static_cast<uint8_t>(LoginStatus::BAD_VERSION):    msg = "Sai phien ban client"; break;
            case LOGIN_NET_ERROR:                                   msg = "Khong the ket noi may chu"; break;
        }
        SDL_SetRenderScale(state.renderer, 2.0f, 2.0f);
        SDL_SetRenderDrawColor(state.renderer, 255, 80, 80, 255);
        SDL_RenderDebugText(state.renderer, 320.0f, 310.0f, msg);
        SDL_SetRenderScale(state.renderer, 1.0f, 1.0f);
    }
}
