#pragma once
#include <vector>
#include <string>
#include <SDL3/SDL.h>
#include "GameObject.h"
#include "SDLState.h"
#include "AnimationTypes.h"

class Resources
{
public:
    std::vector<AnimationInfo> playerAnimations;
    std::vector<AnimationInfo> enemyAnimations;
    std::vector<AnimationInfo> groundAnimations;
    std::vector<AnimationInfo> decorationAnimations;
    std::vector<SDL_Texture*>  textures;

    SDL_Texture* loadTexture(SDL_Renderer *renderer, const std::string &filepath);
    
    void loadAnim(SDL_Renderer* renderer, std::vector<AnimationInfo>& destination,
                  int index, const std::string& path,
                  float frameTime, int frameCount, int cols, int rows);
    void loadPlayerAnimations(SDL_Renderer *renderer);
    void loadGroundAnimations(SDL_Renderer *renderer);
    void loadDecorationAnimations(SDL_Renderer *renderer);
    void load(SDLState &state);
    void unload();
};
