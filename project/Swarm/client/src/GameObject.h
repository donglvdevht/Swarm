#pragma once
#include <array>
#include <glm/glm.hpp>
#include <vector>
#include <SDL3/SDL.h>
#include "Animation.h"

struct AnimationInfo
{
    Animation anim;          // logic chạy frame
    int columns;             // số cột trong sheet
    int rows;                // số hàng trong sheet
    float frameWidth;        // chiều rộng 1 frame
    float frameHeight;       // chiều cao 1 frame
    SDL_Texture *texture;
    void clear()
        {
            columns = 0;
            rows = 0;
            frameWidth = 0.0f;
            frameHeight = 0.0f;
            anim.reset();
            texture = nullptr;
        }
};

struct PropConfig {
    float targetHeight;
    float footHeight;
    float hitboxRadius;
    int footprintTiles;
};

static const std::array<PropConfig, 11> propConfigs = {
    PropConfig{ 100.0f, 100.0f, 0.0f, 1 }, // None
    PropConfig{ 120.0f, 112.0f, 20.0f, 1 }, // Tree Small
    PropConfig{ 180.0f, 170.0f, 28.0f, 1 }, // Tree Medium
    PropConfig{ 250.0f, 231.0f, 40.0f, 4 }, // Tree Large (2x2)
    PropConfig{  60.0f,  54.0f,  0.0f, 1 }, // Bushes Small - không chặn
    PropConfig{  90.0f,  83.0f, 22.0f, 1 }, // Bushes Medium
    PropConfig{ 130.0f, 122.0f, 30.0f, 1 }, // Bushes Large
    PropConfig{ 150.0f, 139.0f, 45.0f, 2 }, // Tent (2x1)
    PropConfig{  80.0f,  80.0f, 18.0f, 1 }, // Campfire
    PropConfig{ 300.0f, 289.0f, 35.0f, 4 }, // Magic Stone Tower (2x2)
    PropConfig{ 120.0f, 110.0f, 35.0f, 1 }, // Well
};

enum class ObjectType
{
    player,
    level,
    enemy
};

enum class Direction
{
    Up,
    Down,
    Left,
    Right
};

class GameObject
{
private:
    std::vector<AnimationInfo> playerAnimations;
    std::vector<AnimationInfo> enemyAnimations;
    std::vector<AnimationInfo> groundAnimations;
    std::vector<AnimationInfo> decorationAnimations;

    glm::vec2 position;
    int currentAnimation;
    ObjectType type;
    Direction facingDirection;
    bool attacking;
    float attackTimeRemaining;
    static constexpr float WALK_SPEED = 100.0f;
    static constexpr float RUN_SPEED = 260.0f;

public:
    GameObject();

    // Getters
    const glm::vec2& getPosition() const { return position; }
    int getCurrentAnimation() const { return currentAnimation; }
    const std::vector<AnimationInfo>& getAnimations() const { return playerAnimations; }
    std::vector<AnimationInfo>& getPlayerAnimations() { return playerAnimations; }
    std::vector<AnimationInfo>& getGroundAnimations() { return groundAnimations; }
    std::vector<AnimationInfo>& getDecorationAnimations() { return decorationAnimations; }

    Direction getFacingDirection() const { return facingDirection; }
    ObjectType getType() const { return type; }
    bool isAttacking() const { return attacking; }

    // Setters
    void setPlayerAnimations(const std::vector<AnimationInfo>& anims) { playerAnimations = anims; }
    void setGroundAnimations(const std::vector<AnimationInfo>& anims) { groundAnimations = anims; }
    void setDecorationAnimations(const std::vector<AnimationInfo>& anims) { decorationAnimations = anims; }

    void setCurrentAnimation(int anim) { currentAnimation = anim; }
    void setPosition(const glm::vec2& pos) { position = pos; }
    void setType(const ObjectType &oType) {type = oType;}

    // Action methods
    void attack();
    void updateAttack(float deltaTime);
    void move(const glm::vec2& direction, float deltaTime, bool running = false);

};
