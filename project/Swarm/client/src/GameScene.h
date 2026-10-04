#pragma once
#include <vector>
#include <deque>
#include <fstream>
#include <nlohmann/json.hpp>

#include "Scene.h"
#include "InputHandler.h"
#include "GameObject.h"
#include "Resources.h"
#include "Network.h"

struct GameState
{
    // ground, static objects, characters
    std::array<std::vector<GameObject>, 3> layer;
    int playerIndex;

    GameState()
    {
        playerIndex = 0;
    }
};

struct RenderItem
{
    float depth;
    bool isPlayer;
    GameObject* obj;
};

class GameScene : public Scene {
public:
    GameScene(const SDLState& state, const Resources& res, Network& net);
    void handleEvent(const SDL_Event& e) override;
    void update(float deltaTime) override;
    void render(const SDLState& state, float deltaTime) override;
    void endFrame() override { inputHandler.update(); }
private:
    InputHandler inputHandler;
    GameState gameState;
    GameObject player;
    Network& network;
    bool hasAckedSeq = false;
    uint32_t lastAckedSeq = 0;
    glm::vec2 correctionTarget{0.0f};
    bool hasCorrection = false;
};