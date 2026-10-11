#include "LoginScene.h"

#include <imgui.h>

namespace
{
    const char* getErrorMessage(uint8_t status)
    {
        switch (status)
        {
            case static_cast<uint8_t>(LoginStatus::WRONG_PASSWORD): return "Incorrect password.";
            case static_cast<uint8_t>(LoginStatus::NAME_TAKEN):     return "Account not found.";
            case static_cast<uint8_t>(LoginStatus::SERVER_FULL):    return "The server is full.";
            case static_cast<uint8_t>(LoginStatus::BAD_VERSION):    return "Client version mismatch.";
            case LOGIN_NET_ERROR:                                   return "Could not connect to the server.";
            default:                                                return "Login failed.";
        }
    }
}

LoginScene::LoginScene(Network& net) : network_(net)
{
}

void LoginScene::handleEvent(const SDL_Event&)
{
}

void LoginScene::submit()
{
    if (username_[0] == '\0' || password_[0] == '\0')
    {
        validationError_ = true;
        return;
    }

    validationError_ = false;
    network_.beginLogin(username_.data(), password_.data());
    formState_ = FormState::Submitting;
}

void LoginScene::update(float)
{
    if (formState_ != FormState::Submitting)
        return;

    switch (network_.getConnState())
    {
        case ConnState::Connected:
            SceneNext_ = SceneId::Game;
            break;
        case ConnState::Failed:
            formState_ = FormState::Editing;
            switch (network_.getLastStatus())
            {
                case static_cast<uint8_t>(LoginStatus::WRONG_PASSWORD):
                    password_.fill('\0');
                    break;
                case static_cast<uint8_t>(LoginStatus::NAME_TAKEN):
                    username_.fill('\0');
                    password_.fill('\0');
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

void LoginScene::render(const SDLState&, float)
{
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        ImGuiCond_Always,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440.0f, 360.0f), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32.0f, 28.0f));
    const bool visible = ImGui::Begin(
        "Sign in",
        nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoTitleBar);

    bool submitOnEnter = false;
    if (visible)
    {
        ImGui::TextUnformatted("SWARM");
        ImGui::SameLine();
        ImGui::TextDisabled("Sign in to continue");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextUnformatted("Username");
        ImGui::SetNextItemWidth(-1.0f);
        const bool focusPassword = ImGui::InputText(
            "##username",
            username_.data(),
            username_.size(),
            ImGuiInputTextFlags_EnterReturnsTrue);
        if (focusPassword)
            ImGui::SetKeyboardFocusHere();

        ImGui::Spacing();
        ImGui::TextUnformatted("Password");
        ImGui::SetNextItemWidth(-1.0f);
        submitOnEnter = ImGui::InputText(
            "##password",
            password_.data(),
            password_.size(),
            ImGuiInputTextFlags_Password |
            ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::BeginDisabled(formState_ == FormState::Submitting);
        const bool clicked = ImGui::Button(
            formState_ == FormState::Submitting ? "Connecting..." : "Sign in",
            ImVec2(-1.0f, 42.0f));
        ImGui::EndDisabled();

        if (clicked || submitOnEnter)
            submit();

        if (validationError_)
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.38f, 1.0f),
                               "Enter both username and password.");
        }
        else if (formState_ == FormState::Submitting)
        {
            ImGui::Spacing();
            ImGui::TextDisabled("Connecting to the server...");
        }
        else if (network_.getConnState() == ConnState::Failed)
        {
            ImGui::Spacing();
            ImGui::TextColored(
                ImVec4(1.0f, 0.42f, 0.38f, 1.0f),
                "%s",
                getErrorMessage(network_.getLastStatus()));
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}
