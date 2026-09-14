#include <cstdio>
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL_stdinc.h>
#include <array>
#include <vector>
#include <string>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include "AnimationData.h"
#include "Animation.h"
#include "GameObject.h"
#include "Resources.h"
#include "SDLState.h"
#include "AnimationTypes.h"
#include "InputHandler.h"
#include "Protocol.h"
using json = nlohmann::json;

#ifndef SWARM_ASSET_DIR
#define SWARM_ASSET_DIR "."
#endif

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define MAP_ROWS 11
#define MAP_COL 20

const float TILE_SIZE = 100.0f;
const float PLAYER_SIZE = TILE_SIZE * 1.5f;
constexpr uint64_t TARGET_FRAME_TIME_MS = 16;

struct InputState
{
    bool left  = false;
    bool right = false;
    bool up    = false;
    bool down  = false;
    bool attack = false;
    bool jump  = false;
};

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

void cleanup(SDLState &state);
bool initialize(SDLState &state);
void drawObject (const SDLState &state, GameObject &obj, float deltaTime);
void drawDecorationObject(const SDLState &state, GameObject &obj);
void drawMap(const SDLState &state, GameState &gs);
PlayerAnim resolvePlayerAnim(InputHandler& input, const GameObject& player, int currentAnimation);
void createTiles(const SDLState &state, GameState &gs, const Resources &res);

struct RenderItem
{
    float depth;
    bool isPlayer;
    GameObject* obj;
};

void sortRenderOrder(std::vector<RenderItem>& items)
{
    std::sort(items.begin(), items.end(), [](const RenderItem& a, const RenderItem& b) {
        return a.depth < b.depth;
    });
}

int main() {
    // --- Create UDP socket ---
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    // --- Set non-blocking ---
    int flag = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flag | O_NONBLOCK);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9000);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr));

    // --- Create the window ---
    SDLState state;
    if (initialize(state))
        return -1;

    Resources res;
    res.load(state);

    GameState gameState;
    createTiles(state, gameState, res);

    // --- Setup gamedata
    GameObject player;
    player.setPlayerAnimations(res.playerAnimations);
    player.setCurrentAnimation((int)PlayerAnim::IdleDown);
    player.setPosition(glm::vec2(WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f));

    uint64_t prevTime = SDL_GetTicks();
    uint64_t lastInputSendTime = 0;
    InputState input;
    InputHandler inputHandler;

    // --- Start the game loop ---
    bool running = true;
    while (running)
    {
        uint64_t nowTime = SDL_GetTicks();
        float deltaTime = (nowTime - prevTime) / 1000.0f;
        deltaTime = std::min(deltaTime, 0.1f);


        SDL_Event event{0};
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                {
                    inputHandler.handleEvent(event);
                    break;
                }
            }
        }

        const bool moveUp = inputHandler.isHeld(InputHandler::Action::MoveUp);
        const bool moveDown = inputHandler.isHeld(InputHandler::Action::MoveDown);
        const bool moveLeft = inputHandler.isHeld(InputHandler::Action::MoveLeft);
        const bool moveRight = inputHandler.isHeld(InputHandler::Action::MoveRight);
        const bool runHeld = inputHandler.isHeld(InputHandler::Action::Run);
        const bool attackPressed = inputHandler.isPressed(InputHandler::Action::Attack);

        // --- Gửi packet định kì để sever biết client còn sống
        if (attackPressed || nowTime - lastInputSendTime >= 50)
        {
            PlayerInputPacket packet{};
            packet.header.type = PacketType::PLAYER_INPUT;
            packet.header.tick = htonl(static_cast<uint32_t>(nowTime));
            packet.up = moveUp;
            packet.down = moveDown;
            packet.left = moveLeft;
            packet.right = moveRight;
            packet.attack = attackPressed;
            packet.run = runHeld;

            const ssize_t sent = send(sock, &packet, sizeof(packet), 0);
            if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("send");
            }
            else if (sent == static_cast<ssize_t>(sizeof(packet))) {
                lastInputSendTime = nowTime;
            }
        }

        char buffer[1500];
        while (true)
        {
            const ssize_t n = recv(sock, buffer, sizeof(buffer), 0);
            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break;  // No data
                }
                perror("recv");
                break;
            }

            if (n == sizeof(EchoReplyPagket))
            {
                EchoReplyPagket reply{};
                memcpy(&reply, buffer, sizeof(reply));
                if (reply.header.type == PacketType::ECHO_REPLY)
                {
                    const uint32_t tick = ntohl(reply.header.tick);
                    printf("EchoReply tick = %u\n", tick);
                }
            }
        }

        glm::vec2 moveDir{0.0f, 0.0f};
        if (moveUp)    moveDir.y -= 1.0f;
        if (moveDown)  moveDir.y += 1.0f;
        if (moveLeft)  moveDir.x -= 1.0f;
        if (moveRight) moveDir.x += 1.0f;

        if (glm::length(moveDir) > 0.0f) {
            player.move(glm::normalize(moveDir), deltaTime, runHeld);
        }

        if (attackPressed) {
            player.attack();
        }

        player.updateAttack(deltaTime);

        PlayerAnim nextAnimation;
        nextAnimation = resolvePlayerAnim(inputHandler, player, player.getCurrentAnimation());

        // --- Switch animation only when changing ---
        if ((int)nextAnimation != player.getCurrentAnimation())
        {
            player.setCurrentAnimation((int)nextAnimation);
            player.getPlayerAnimations()[player.getCurrentAnimation()].anim.reset();
        }

        // --- Vẽ ---
        SDL_SetRenderDrawColor(state.renderer, 20, 10, 30, 255);
        SDL_RenderClear(state.renderer);

        drawMap(state, gameState);

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
        sortRenderOrder(renderOrder);

        for (const RenderItem& item : renderOrder) {
            if (item.isPlayer) {
                drawObject(state, *item.obj, deltaTime);
            } else {
                drawDecorationObject(state, *item.obj);
            }
        }

        SDL_RenderPresent(state.renderer);
        inputHandler.update();

        const uint64_t frameTime = SDL_GetTicks() - nowTime;
        if (frameTime < TARGET_FRAME_TIME_MS) {
            SDL_Delay(static_cast<uint32_t>(TARGET_FRAME_TIME_MS - frameTime));
        }
        prevTime = nowTime;
    }

    res.unload();
    cleanup(state);
    return 0;
}

void cleanup(SDLState &state)
{
    SDL_DestroyRenderer(state.renderer);
    SDL_DestroyWindow(state.window);
    SDL_Quit();
}

bool initialize(SDLState &state)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error initializing SDL3", nullptr);
        return 1;
    }
    state.window = SDL_CreateWindow("Swarm", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!state.window) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating window", nullptr);
        cleanup(state);
        return 1;
    }

    state.renderer = SDL_CreateRenderer(state.window, nullptr);
    if (!state.renderer)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating renderer", nullptr);
        cleanup(state);
        return 1;
    }

    int width = 1920;
    int height = 1080;
    SDL_SetRenderLogicalPresentation(state.renderer, width, height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    return 0;
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

    AnimationInfo& info = animations[animIndex];
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

    SDL_FRect destRect{position.x, position.y, targetSize, targetSize};
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

        AnimationInfo& info = animations[animIndex];
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

        AnimationInfo& info = animations[animIndex];
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
        SDL_FRect destRect{position.x, position.y, targetSize, targetSize};

        SDL_RenderTexture(state.renderer, info.texture, nullptr, &destRect);
    }
}

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
        o.setGroundAnimations(res.groundAnimations);
        return o;
    };

    const auto createDecorationTile = [&res](int r, int c, ObjectType type, short value)
    {
        GameObject o;
        o.setType(type);
        o.setPosition(glm::vec2(c * TILE_SIZE, r * TILE_SIZE));
        o.setCurrentAnimation(value);
        o.setDecorationAnimations(res.decorationAnimations);
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
