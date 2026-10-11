#include "LoginScene.h"

#include <algorithm>
#include <string_view>

namespace
{
    constexpr SDL_FRect kPanelRect{350.0f, 110.0f, 580.0f, 500.0f};

    SDL_FRect fieldRect(float x, float y, float w, float h)
    {
        return {x, y, w, h};
    }

    const char* getPlaceholder(const TextField& field)
    {
        return field.isPassword ? "Nhập mật khẩu" : "Nhập tên đăng nhập";
    }

    const char* getErrorMessage(uint8_t status)
    {
        switch (status)
        {
            case static_cast<uint8_t>(LoginStatus::WRONG_PASSWORD): return "Mật khẩu không đúng";
            case static_cast<uint8_t>(LoginStatus::NAME_TAKEN):     return "Tài khoản không tồn tại";
            case static_cast<uint8_t>(LoginStatus::SERVER_FULL):    return "Máy chủ đã đầy";
            case static_cast<uint8_t>(LoginStatus::BAD_VERSION):    return "Sai phiên bản client";
            case LOGIN_NET_ERROR:                                   return "Không thể kết nối máy chủ";
            default:                                               return "Đăng nhập thất bại";
        }
    }
}

LoginScene::LoginScene(Network& net) : network_(net)
{
    textField_.resize(2);

    textField_[0] = TextField{
        fieldRect(430.0f, 260.0f, 420.0f, 70.0f),
        "Tên đăng nhập",
        "",
        false,
        false,
        MAX_NAME_LEN
    };

    textField_[1] = TextField{
        fieldRect(430.0f, 360.0f, 420.0f, 70.0f),
        "Mật khẩu",
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
    SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(state.renderer, 15, 18, 28, 255);
    SDL_RenderClear(state.renderer);

    const SDL_FRect backgroundRect{0.0f, 0.0f, 1280.0f, 720.0f};
    SDL_SetRenderDrawColor(state.renderer, 12, 18, 30, 255);
    SDL_RenderFillRect(state.renderer, &backgroundRect);

    SDL_SetRenderDrawColor(state.renderer, 33, 42, 66, 255);
    SDL_RenderFillRect(state.renderer, &kPanelRect);

    SDL_SetRenderDrawColor(state.renderer, 79, 170, 255, 255);
    SDL_FRect accentRect{kPanelRect.x, kPanelRect.y, kPanelRect.w, 6.0f};
    SDL_RenderFillRect(state.renderer, &accentRect);

    SDL_SetRenderDrawColor(state.renderer, 72, 85, 112, 255);
    SDL_RenderRect(state.renderer, &kPanelRect);

    SDL_SetRenderScale(state.renderer, 1.7f, 1.7f);
    SDL_SetRenderDrawColor(state.renderer, 245, 247, 250, 255);
    SDL_RenderDebugText(state.renderer, 220.0f, 102.0f, "Đăng nhập");

    SDL_SetRenderDrawColor(state.renderer, 160, 175, 200, 255);
    SDL_RenderDebugText(state.renderer, 220.0f, 140.0f, "Chào mừng bạn quay lại");
    SDL_SetRenderScale(state.renderer, 1.0f, 1.0f);

    for (const TextField& txtFld : textField_)
    {
        if (txtFld.isFilled)
            SDL_SetRenderDrawColor(state.renderer, 92, 160, 255, 255);
        else
            SDL_SetRenderDrawColor(state.renderer, 39, 49, 64, 255);

        SDL_RenderFillRect(state.renderer, &txtFld.rect);

        SDL_SetRenderDrawColor(state.renderer, 128, 155, 184, 255);
        SDL_RenderRect(state.renderer, &txtFld.rect);

        SDL_SetRenderDrawColor(state.renderer, 186, 207, 232, 255);
        SDL_RenderDebugText(state.renderer,
                            txtFld.rect.x + 14.0f,
                            txtFld.rect.y + 6.0f,
                            txtFld.label.c_str());

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
            displayText = getPlaceholder(txtFld);

        SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(state.renderer,
                            txtFld.rect.x + 14.0f,
                            txtFld.rect.y + 34.0f,
                            displayText.c_str());
    }

    SDL_SetRenderDrawColor(state.renderer, 158, 171, 196, 255);
    SDL_RenderDebugText(state.renderer, 430.0f, 460.0f, "Mẹo: bạn có thể dùng tên tài khoản đã đăng ký trước đó.");

    SDL_SetRenderDrawColor(state.renderer, 82, 160, 255, 255);
    SDL_RenderDebugText(state.renderer, 430.0f, 540.0f, "Đăng nhập");

    if (network_.getConnState() == ConnState::Failed)
    {
        const char* msg = getErrorMessage(network_.getLastStatus());
        SDL_SetRenderDrawColor(state.renderer, 255, 93, 92, 255);
        SDL_RenderDebugText(state.renderer, 430.0f, 610.0f, msg);
    }
}
