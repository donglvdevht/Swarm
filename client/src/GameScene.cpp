#include "GameScene.h"
#include "GameConstants.h"

#ifndef SWARM_ASSET_DIR
#define SWARM_ASSET_DIR "."
#endif

using json = nlohmann::json;

namespace GameSceneFunc
{
    void createTiles(const SDLState &state, GameState &gs, const Resources &res)
    {
        const std::string mapPath = std::string(SWARM_ASSET_DIR) + "/map.json";
        std::ifstream file(mapPath);
        if (!file.is_open()) {
            std::fprintf(stderr, "Failed to open map file: %s\n", mapPath.c_str());
            return;
        }

        json data;
        file >> data;
        int rows = data["rows"];
        int cols = data["cols"];

        const auto createGroundTile = [&res](int r, int c, ObjectType type, short value)
        {
            GameObject o;
            o.setType(type);
            o.setPosition(glm::vec2(c * TILE_SIZE, r * TILE_SIZE));
            o.setCurrentAnimation(value);
            o.setGroundAnimations(&res.groundAnimations);
            return o;
        };

        const auto createDecorationTile = [&res](int r, int c, ObjectType type, short value)
        {
            GameObject o;
            o.setType(type);
            o.setPosition(glm::vec2(c * TILE_SIZE, r * TILE_SIZE));
            o.setCurrentAnimation(value);
            o.setDecorationAnimations(&res.decorationAnimations);
            return o;
        };

        for (int r = 0; r < MAP_ROWS; r++)
        {
            for (int c = 0; c < MAP_COL; c++)
            {
                if (data["groundMap"][r][c] != 0) {
                    gs.layer[0].push_back(createGroundTile(r, c, ObjectType::level, data["groundMap"][r][c]));
                }

                if (data["objectMap"][r][c] != 0) {
                    gs.layer[1].push_back(createDecorationTile(r, c, ObjectType::level, data["objectMap"][r][c]));
                }
            }
        }
    }

    PlayerAnim resolvePlayerAnim(InputHandler& input, const GameObject& player, int currentAnimation)
    {
        const bool attacking = player.isAttacking();

        if (attacking) {
            switch (player.getFacingDirection()) {
                case Direction::Up:    return PlayerAnim::AttackUp;
                case Direction::Left:  return PlayerAnim::AttackLeft;
                case Direction::Right: return PlayerAnim::AttackRight;
                case Direction::Down:
                default:              return PlayerAnim::AttackDown;
            }
        }

        const bool running = input.isHeld(InputHandler::Action::Run);
        if (running) {
            if (input.isHeld(InputHandler::Action::MoveUp))    return PlayerAnim::RunUp;
            if (input.isHeld(InputHandler::Action::MoveDown))  return PlayerAnim::RunDown;
            if (input.isHeld(InputHandler::Action::MoveLeft))  return PlayerAnim::RunLeft;
            if (input.isHeld(InputHandler::Action::MoveRight)) return PlayerAnim::RunRight;
        }

        if (input.isHeld(InputHandler::Action::MoveUp))    return PlayerAnim::WalkUp;
        if (input.isHeld(InputHandler::Action::MoveDown))  return PlayerAnim::WalkDown;
        if (input.isHeld(InputHandler::Action::MoveLeft))  return PlayerAnim::WalkLeft;
        if (input.isHeld(InputHandler::Action::MoveRight)) return PlayerAnim::WalkRight;

        switch (player.getFacingDirection()) {
            case Direction::Up:    return PlayerAnim::IdleUp;
            case Direction::Left:  return PlayerAnim::IdleLeft;
            case Direction::Right: return PlayerAnim::IdleRight;
            case Direction::Down:
            default:              return PlayerAnim::IdleDown;
        }
    }

    void drawObject (const SDLState &state, GameObject &obj, float deltaTime)
    {
            auto& animations = obj.getPlayerAnimations();
            int currentAnimation = obj.getCurrentAnimation();
        if (currentAnimation < 0 || currentAnimation >= static_cast<int>(animations.size())) {
            return;
        }
            AnimationInfo& info = animations[currentAnimation];
            if (!info.texture || info.columns <= 0 || info.rows <= 0) {
                return;
            }
            // --- update animation theo deltaTime ---
            info.anim.update(deltaTime);

            int frameIndex = currentAnimation != -1
                ? info.anim.getCurrentFrame()
                : 0;

            int currentCol = frameIndex % info.columns;
            int currentRow = frameIndex / info.columns;

            // --- srcRect: cắt frame từ ảnh gốc ---
            SDL_FRect srcRect = {
                currentCol * info.frameWidth,   // x = cột * chiều rộng 1 frame
                currentRow * info.frameHeight,   // y = hàng * chiều cao 1 frame
                info.frameWidth,
                info.frameHeight
            };

            // --- destRect: vị trí + kích thước hiển thị trên màn hình ---
            glm::vec2 pos = obj.getPosition();
            SDL_FRect destRect = {
                pos.x - PLAYER_SIZE / 2.0f,    // vị trí x (centered)
                pos.y - PLAYER_SIZE / 2.0f,    // vị trí y (centered)
                PLAYER_SIZE,
                PLAYER_SIZE
            };

            SDL_RenderTexture(state.renderer, info.texture, &srcRect, &destRect);
    }

    void drawDecorationObject(const SDLState &state, GameObject &obj)
    {
        auto& animations = obj.getDecorationAnimations();
        const int animIndex = obj.getCurrentAnimation();
        if (animIndex < 0 || animIndex >= static_cast<int>(animations.size())) {
            return;
        }

        const AnimationInfo& info = animations[animIndex];
        if (!info.texture) {
            return;
        }

        const glm::vec2 position = obj.getPosition();
        SDL_FRect srcRect = {
            0,
            0,
            info.frameWidth,
            info.frameHeight
        };

        float targetSize = TILE_SIZE;
        if (animIndex >= 0 && animIndex < static_cast<int>(propConfigs.size())) {
            targetSize = propConfigs[animIndex].targetHeight;
        }

        // --- Vẽ Decoration ---
        const float aspect = (info.frameHeight > 0.0f) ? (info.frameWidth / info.frameHeight) : 1.0f;
        SDL_FRect destRect{position.x, position.y, targetSize * aspect, targetSize};
        SDL_RenderTexture(state.renderer, info.texture, &srcRect, &destRect);
    }

    void drawMap(const SDLState &state, GameState &gs)
    {
        for (GameObject& tile : gs.layer[0]) {
            auto& animations = tile.getGroundAnimations();
            const int animIndex = tile.getCurrentAnimation();
            if (animIndex < 0 || animIndex >= static_cast<int>(animations.size())) {
                continue;
            }

            const AnimationInfo& info = animations[animIndex];
            if (!info.texture) {
                continue;
            }

            const glm::vec2 position = tile.getPosition();
            SDL_FRect srcRect = {
                0,
                0,
                info.frameWidth,
                info.frameHeight
            };
            SDL_FRect destRect{position.x, position.y, TILE_SIZE, TILE_SIZE};
            SDL_RenderTexture(state.renderer, info.texture, nullptr, &destRect);
        }

        for (GameObject& object : gs.layer[1]) {
            auto& animations = object.getDecorationAnimations();
            const int animIndex = object.getCurrentAnimation();
            if (animIndex < 0 || animIndex >= static_cast<int>(animations.size())) {
                continue;
            }

            const AnimationInfo& info = animations[animIndex];
            if (!info.texture) {
                continue;
            }
            const glm::vec2 position = object.getPosition();
            SDL_FRect srcRect = {
                0,
                0,
                info.frameWidth,
                info.frameHeight
            };
            float targetSize = TILE_SIZE;
            if (animIndex < static_cast<int>(propConfigs.size())) {
                targetSize = propConfigs[animIndex].targetHeight;
            }
            // --- Giữ đúng tỉ lệ khung hình gốc, không ép vuông ---
            const float aspect = (info.frameHeight > 0.0f) ? (info.frameWidth / info.frameHeight) : 1.0f;
            SDL_FRect destRect{position.x, position.y, targetSize * aspect, targetSize};

            SDL_RenderTexture(state.renderer, info.texture, nullptr, &destRect);
        }
    }

    void sortRenderOrder(std::vector<RenderItem>& items)
    {
        std::sort(items.begin(), items.end(), [](const RenderItem& a, const RenderItem& b) {
            return a.depth < b.depth;
        });
    }
}

GameScene::GameScene(const SDLState& state, const Resources& res, Network& net) : network(net)
{
    GameSceneFunc::createTiles(state, gameState, res);
    player.setPlayerAnimations(res.playerAnimations);
    player.setCurrentAnimation((int)PlayerAnim::IdleDown);
    player.setPosition(glm::vec2(state.logicalWidth / 2.0f, state.logicalHeight / 2.0f));
}

void GameScene::handleEvent(const SDL_Event& e)
{
    inputHandler.handleEvent(e);
}

void GameScene::update(float deltaTime)
{
    const bool moveUp = inputHandler.isHeld(InputHandler::Action::MoveUp);
    const bool moveDown = inputHandler.isHeld(InputHandler::Action::MoveDown);
    const bool moveLeft = inputHandler.isHeld(InputHandler::Action::MoveLeft);
    const bool moveRight = inputHandler.isHeld(InputHandler::Action::MoveRight);
    const bool runHeld = inputHandler.isHeld(InputHandler::Action::Run);
    const bool attackPressed = inputHandler.isPressed(InputHandler::Action::Attack);

    network.updateInput(moveUp, moveDown, moveLeft, moveRight, attackPressed, runHeld);

    glm::vec2 moveDir{0.0f, 0.0f};
    if (moveUp)    moveDir.y -= 1.0f;
    if (moveDown)  moveDir.y += 1.0f;
    if (moveLeft)  moveDir.x -= 1.0f;
    if (moveRight) moveDir.x += 1.0f;

    if (glm::length(moveDir) > 0.0f) {
        player.move(glm::normalize(moveDir), deltaTime, runHeld);
    }

    // --- Reconciliation: chỉ chạy khi có state mới từ server ---
    const ServerState serverState = network.getSeverState();
    if (serverState.valid && (!hasAckedSeq || serverState.lastProcessedSeq != lastAckedSeq))
    {
        hasAckedSeq = true;
        lastAckedSeq = serverState.lastProcessedSeq;

        glm::vec2 target(serverState.x, serverState.y);
        for (const SentInput& inp : network.getUnackedInputs(serverState.lastProcessedSeq))
        {
            glm::vec2 dir{0.0f, 0.0f};
            if (inp.up)    dir.y -= 1.0f;
            if (inp.down)  dir.y += 1.0f;
            if (inp.left)  dir.x -= 1.0f;
            if (inp.right) dir.x += 1.0f;
            if (glm::length(dir) > 0.0001f)
                target += glm::normalize(dir) * (inp.run ? GameObject::RUN_SPEED : GameObject::WALK_SPEED) * FIXED_DT;
        }
        correctionTarget = target;
        hasCorrection = true;
    }

    if (hasCorrection)
    {
        const glm::vec2 err = correctionTarget - player.getPosition();
        const float dist = glm::length(err);
        constexpr float DEAD_ZONE = GameObject::RUN_SPEED * FIXED_DT * 1.5f;  // ~ 1.5 tick chạy
        constexpr float SNAP_DIST = 150.0f;
        if (dist > SNAP_DIST) {
            player.setPosition(correctionTarget);
            hasCorrection = false;
        } else if (dist > DEAD_ZONE) {
            player.setPosition(player.getPosition() + err * std::min(1.0f, 10.0f * deltaTime));
        } else {
            hasCorrection = false;
        }
    }

    if (attackPressed) {
        player.attack();
    }

    player.updateAttack(deltaTime);

    PlayerAnim nextAnimation;
    nextAnimation = GameSceneFunc::resolvePlayerAnim(inputHandler, player, player.getCurrentAnimation());

    // --- Switch animation only when changing ---
    if ((int)nextAnimation != player.getCurrentAnimation())
    {
        player.setCurrentAnimation((int)nextAnimation);
        player.getPlayerAnimations()[player.getCurrentAnimation()].anim.reset();
    }
}

void GameScene::render(const SDLState& state, float deltaTime)
{
    GameSceneFunc::drawMap(state, gameState);

    std::vector<RenderItem> renderOrder;
    for (GameObject& object : gameState.layer[1]) {
        const int animIndex = object.getCurrentAnimation();
        const float footHeight = animIndex >= 0 &&
            animIndex < static_cast<int>(propConfigs.size())
            ? propConfigs[animIndex].footHeight
            : TILE_SIZE;
        const float objDepth = object.getPosition().y + footHeight;
        renderOrder.push_back({ objDepth, false, &object });
    }
    renderOrder.push_back({ player.getPosition().y + PLAYER_SIZE * 0.5f, true, &player });
    GameSceneFunc::sortRenderOrder(renderOrder);

    for (const RenderItem& item : renderOrder) {
        if (item.isPlayer) {
            GameSceneFunc::drawObject(state, *item.obj, deltaTime);
        } else {
            GameSceneFunc::drawDecorationObject(state, *item.obj);
        }
    }
}
