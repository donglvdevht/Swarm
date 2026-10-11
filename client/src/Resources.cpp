#include "Resources.h"
#include "AnimationTypes.h"
#include <cstdio>
#include <SDL3_image/SDL_image.h>
#include "AnimationData.h"
#include "Animation.h"
#include <iomanip>
#include <sstream>

#ifndef SWARM_ASSET_DIR
#define SWARM_ASSET_DIR "."
#endif

namespace
{
std::string assetPath(const std::string& relativePath)
{
    return std::string(SWARM_ASSET_DIR) + "/" + relativePath;
}
}

std::string intToString2(int number)
{
    std::stringstream ss;
    ss << std::setw(2) << std::setfill('0') << number;
    return ss.str();
}

SDL_Texture* Resources::loadTexture(SDL_Renderer *renderer, const std::string &filepath)
{
    SDL_Texture *tex = IMG_LoadTexture(renderer, filepath.c_str());
    if (!tex) {
        fprintf(stderr, "Failed to load texture: %s\n", filepath.c_str());
        return nullptr;
    }
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
    textures.push_back(tex);
    return tex;
}

void Resources::loadAnim(SDL_Renderer* renderer, std::vector<AnimationInfo>& destination,
                         int index, const std::string& path,
                         float frameTime, int frameCount, int cols, int rows)
{
    AnimationInfo info{};
    info.texture = loadTexture(renderer, path);
    if (!info.texture) {
        fprintf(stderr, "Skipping invalid texture: %s\n", path.c_str());
        return;
    }

    info.anim    = Animation(frameTime, frameCount);
    info.columns = cols;
    info.rows    = rows;

    float w, h;
    SDL_GetTextureSize(info.texture, &w, &h);
    info.frameWidth  = w / cols;
    info.frameHeight = h / rows;

    destination[index] = info;
}

void Resources::loadPlayerAnimations(SDL_Renderer *renderer)
{
    // Pre-allocate vector for all playerAnimations
    playerAnimations.resize((int)PlayerAnim::Count);
    
    std::string path;

    // ===== IDLE DOWN =====
    path = assetPath("assets/players/AztecLeader/Spritesheeets/Front - Idle.png");
    loadAnim(renderer, playerAnimations, (int)PlayerAnim::IdleDown, path,
             DefPlayerAnim::FRONT_IDLE_FRAME_TIME,
             DefPlayerAnim::FRONT_IDLE_FRAMES, 4, 4);

    // ===== IDLE UP =====
    path = assetPath("assets/players/AztecLeader/Spritesheeets/Back - Idle.png");
    loadAnim(renderer, playerAnimations, (int)PlayerAnim::IdleUp, path,
             DefPlayerAnim::FRONT_IDLE_FRAME_TIME,
             DefPlayerAnim::FRONT_IDLE_FRAMES, 4, 4);

    // ===== IDLE LEFT =====
    path = assetPath("assets/players/AztecLeader/Spritesheeets/Left - Idle.png");
    loadAnim(renderer, playerAnimations, (int)PlayerAnim::IdleLeft, path,
             DefPlayerAnim::FRONT_IDLE_FRAME_TIME,
             DefPlayerAnim::FRONT_IDLE_FRAMES, 4, 4);

    // ===== IDLE RIGHT =====
    path = assetPath("assets/players/AztecLeader/Spritesheeets/Right - Idle.png");
    loadAnim(renderer, playerAnimations, (int)PlayerAnim::IdleRight, path,
             DefPlayerAnim::FRONT_IDLE_FRAME_TIME,
             DefPlayerAnim::FRONT_IDLE_FRAMES, 4, 4);

    // ===== WALK DOWN =====
    path = assetPath("assets/players/AztecLeader/Spritesheeets/Front - Walking.png");
    loadAnim(renderer, playerAnimations, (int)PlayerAnim::WalkDown, path,
             DefPlayerAnim::FRONT_WALKING_FRAME_TIME,
             DefPlayerAnim::FRONT_WALKING_FRAMES, 4, 5);

    // ===== WALK UP =====
    path = assetPath("assets/players/AztecLeader/Spritesheeets/Back - Walking.png");
    loadAnim(renderer, playerAnimations, (int)PlayerAnim::WalkUp, path,
             DefPlayerAnim::FRONT_WALKING_FRAME_TIME,
             DefPlayerAnim::FRONT_WALKING_FRAMES, 4, 5);

    // ===== WALK LEFT =====
    path = assetPath("assets/players/AztecLeader/Spritesheeets/Left - Walking.png");
    loadAnim(renderer, playerAnimations, (int)PlayerAnim::WalkLeft, path,
             DefPlayerAnim::FRONT_WALKING_FRAME_TIME,
             DefPlayerAnim::FRONT_WALKING_FRAMES, 4, 5);

    // ===== WALK RIGHT =====
    path = assetPath("assets/players/AztecLeader/Spritesheeets/Right - Walking.png");
    loadAnim(renderer, playerAnimations, (int)PlayerAnim::WalkRight, path,
             DefPlayerAnim::FRONT_WALKING_FRAME_TIME,
             DefPlayerAnim::FRONT_WALKING_FRAMES, 4, 5);

    // ===== RUN =====
    path = assetPath("assets/players/AztecLeader/Spritesheeets/Front - Running.png");
    loadAnim(renderer, playerAnimations, (int)PlayerAnim::RunDown, path,
             DefPlayerAnim::FRONT_RUNNING_FRAME_TIME,
             DefPlayerAnim::FRONT_RUNNING_FRAMES, 4, 3);

    path = assetPath("assets/players/AztecLeader/Spritesheeets/Back - Running.png");
    loadAnim(renderer, playerAnimations, (int)PlayerAnim::RunUp, path,
             DefPlayerAnim::FRONT_RUNNING_FRAME_TIME,
             DefPlayerAnim::FRONT_RUNNING_FRAMES, 4, 3);

    path = assetPath("assets/players/AztecLeader/Spritesheeets/Left - Running.png");
    loadAnim(renderer, playerAnimations, (int)PlayerAnim::RunLeft, path,
             DefPlayerAnim::FRONT_RUNNING_FRAME_TIME,
             DefPlayerAnim::FRONT_RUNNING_FRAMES, 4, 3);

    path = assetPath("assets/players/AztecLeader/Spritesheeets/Right - Running.png");
    loadAnim(renderer, playerAnimations, (int)PlayerAnim::RunRight, path,
             DefPlayerAnim::FRONT_RUNNING_FRAME_TIME,
             DefPlayerAnim::FRONT_RUNNING_FRAMES, 4, 3);

    // ===== ATTACK =====
    path = assetPath("assets/players/AztecLeader/Spritesheeets/Front - Attacking.png");
    loadAnim(renderer, playerAnimations, (int)PlayerAnim::AttackDown, path,
             DefPlayerAnim::ATTACK_FRAME_TIME, DefPlayerAnim::ATTACK_FRAMES, 5, 2);

    path = assetPath("assets/players/AztecLeader/Spritesheeets/Back - Attacking.png");
    loadAnim(renderer, playerAnimations, (int)PlayerAnim::AttackUp, path,
             DefPlayerAnim::ATTACK_FRAME_TIME, DefPlayerAnim::ATTACK_FRAMES, 5, 2);

    path = assetPath("assets/players/AztecLeader/Spritesheeets/Left - Attacking.png");
    loadAnim(renderer, playerAnimations, (int)PlayerAnim::AttackLeft, path,
             DefPlayerAnim::ATTACK_FRAME_TIME, DefPlayerAnim::ATTACK_FRAMES, 5, 2);

    path = assetPath("assets/players/AztecLeader/Spritesheeets/Right - Attacking.png");
    loadAnim(renderer, playerAnimations, (int)PlayerAnim::AttackRight, path,
             DefPlayerAnim::ATTACK_FRAME_TIME, DefPlayerAnim::ATTACK_FRAMES, 5, 2);
}

void Resources::loadGroundAnimations(SDL_Renderer *renderer)
{
    const int tileCount = (int)GroundAnim::Count;
    groundAnimations.resize(tileCount + 1);

    std::string path;
    for (int i = 1; i <= tileCount; ++i)
    {
        path = assetPath("assets/map/Top-Down Simple Summer_Ground "
            + intToString2(i) + ".png");

        loadAnim(
            renderer,
            groundAnimations,
            i,
            path,
            0, 1, 1, 1
        );
    }
}

void Resources::loadDecorationAnimations(SDL_Renderer *renderer)
{
    static const std::vector<std::string> propFiles = {
        "",
        assetPath("assets/map/Top-Down Simple Summer_Prop - Tree Small.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Tree Medium.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Tree Large.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Bushes Small.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Bushes Medium.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Bushes Large.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Tent.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Campfire.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Magic Stone Tower.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Well.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Blue Banner.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Castle Round.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Castle Square.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Flag.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - House.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Red Banner.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Rock 01.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Rock 02.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Rock 03.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Rock 04.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Rock 05.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Treasure Chest.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Tree Stump Short.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Tree Stump Tall.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Watchtower Short.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Watchtower Tall.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Windmill.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Wooden Barrel.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Wooden Bridge Horizontal.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Wooden Bridge Vertical.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Wooden Cart.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Wooden Fence Horizontal.png"),
        assetPath("assets/map/Top-Down Simple Summer_Prop - Wooden Fence Vertical.png")
    };

    decorationAnimations.resize(static_cast<size_t>(DecorationAnim::Count));
    for (size_t i = 1; i < propFiles.size(); ++i)
    {
        loadAnim(renderer, decorationAnimations, static_cast<int>(i), propFiles[i], 0, 1, 1, 1);
    }
}

void Resources::load(SDLState &state)
{
    loadPlayerAnimations(state.renderer);
    loadGroundAnimations(state.renderer);
    loadDecorationAnimations(state.renderer);
}

void Resources::unload()
{
    for (SDL_Texture *tex : textures)
    {
        SDL_DestroyTexture(tex);
    }
}
