#pragma once
#include <array>
#include <glm/glm.hpp>
#include <vector>
#include <SDL3/SDL.h>
#include "Animation.h"
#include "AnimationTypes.h"

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

static const std::array<PropConfig, (size_t)DecorationAnim::Count> propConfigs = {
    PropConfig{ 100.0f, 100.0f,  0.0f, 1 }, // None
    PropConfig{ 120.0f, 112.0f, 20.0f, 1 }, // TreeSmall
    PropConfig{ 180.0f, 170.0f, 28.0f, 1 }, // TreeMedium
    PropConfig{ 250.0f, 231.0f, 40.0f, 4 }, // TreeLarge (2x2)
    PropConfig{  60.0f,  54.0f,  0.0f, 1 }, // BushSmall
    PropConfig{  90.0f,  83.0f, 22.0f, 1 }, // BushMedium
    PropConfig{ 130.0f, 122.0f, 30.0f, 1 }, // BushLarge
    PropConfig{ 150.0f, 139.0f, 45.0f, 2 }, // Tent (2x1)
    PropConfig{  80.0f,  80.0f, 18.0f, 1 }, // Campfire
    PropConfig{ 300.0f, 289.0f, 35.0f, 4 }, // MagicStoneTower (2x2)
    PropConfig{ 120.0f, 110.0f, 35.0f, 1 }, // Well
    PropConfig{ 120.0f, 110.0f,  8.0f, 1 }, // BlueBanner
    PropConfig{ 240.0f, 220.0f, 70.0f, 4 }, // CastleRound (2x2)
    PropConfig{ 200.0f, 184.0f, 75.0f, 4 }, // CastleSquare (2x2)
    PropConfig{  80.0f,  74.0f,  6.0f, 1 }, // Flag
    PropConfig{ 320.0f, 294.0f, 90.0f, 4 }, // House (2x2, có thể cần lớn hơn)
    PropConfig{ 120.0f, 110.0f,  8.0f, 1 }, // RedBanner
    PropConfig{  50.0f,  46.0f, 20.0f, 1 }, // Rock01
    PropConfig{  30.0f,  28.0f, 15.0f, 1 }, // Rock02
    PropConfig{  30.0f,  28.0f, 15.0f, 1 }, // Rock03
    PropConfig{  30.0f,  28.0f, 12.0f, 1 }, // Rock04
    PropConfig{  30.0f,  28.0f, 12.0f, 1 }, // Rock05
    PropConfig{  80.0f,  74.0f, 22.0f, 1 }, // TreasureChest
    PropConfig{  50.0f,  46.0f, 18.0f, 1 }, // TreeStumpShort
    PropConfig{  70.0f,  64.0f, 20.0f, 1 }, // TreeStumpTall
    PropConfig{ 130.0f, 120.0f, 40.0f, 2 }, // WatchtowerShort
    PropConfig{ 170.0f, 156.0f, 45.0f, 2 }, // WatchtowerTall
    PropConfig{ 360.0f, 330.0f, 80.0f, 4 }, // Windmill (2x2)
    PropConfig{  70.0f,  64.0f, 18.0f, 1 }, // WoodenBarrel
    PropConfig{ 160.0f, 160.0f,  0.0f, 2 }, // WoodenBridgeHorizontal (đi qua được)
    PropConfig{ 160.0f, 160.0f,  0.0f, 2 }, // WoodenBridgeVertical
    PropConfig{  80.0f,  74.0f, 30.0f, 1 }, // WoodenCart
    PropConfig{  40.0f,  40.0f, 15.0f, 1 }, // WoodenFenceHorizontal
    PropConfig{ 110.0f, 101.0f, 15.0f, 1 }, // WoodenFenceVertical
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
    const std::vector<AnimationInfo>* groundAnimations = nullptr;
    const std::vector<AnimationInfo>* decorationAnimations = nullptr;

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
    std::vector<AnimationInfo>& getPlayerAnimations() { return playerAnimations; }
    const std::vector<AnimationInfo>& getGroundAnimations() const
    {
        static const std::vector<AnimationInfo> empty;
        return groundAnimations ? *groundAnimations : empty;
    }
    const std::vector<AnimationInfo>& getDecorationAnimations() const
    {
        static const std::vector<AnimationInfo> empty;
        return decorationAnimations ? *decorationAnimations : empty;
    }

    Direction getFacingDirection() const { return facingDirection; }
    ObjectType getType() const { return type; }
    bool isAttacking() const { return attacking; }

    // Setters
    void setPlayerAnimations(const std::vector<AnimationInfo>& anims) { playerAnimations = anims; }
    void setGroundAnimations(const std::vector<AnimationInfo>* anims) { groundAnimations = anims; }
    void setDecorationAnimations(const std::vector<AnimationInfo>* anims) { decorationAnimations = anims; }

    void setCurrentAnimation(int anim) { currentAnimation = anim; }
    void setPosition(const glm::vec2& pos) { position = pos; }
    void setType(const ObjectType &oType) {type = oType;}

    // Action methods
    void attack();
    void updateAttack(float deltaTime);
    void move(const glm::vec2& direction, float deltaTime, bool running = false);

};
