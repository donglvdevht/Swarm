#pragma once
#include <string>

// ====================== PLAYER ======================
namespace DefPlayerAnim
{
    // Idle
    constexpr int   FRONT_IDLE_FRAMES      = 16;
    constexpr float FRONT_IDLE_FRAME_TIME  = 0.08f;

    // Walk
    constexpr int   FRONT_WALKING_FRAMES   = 20;
    constexpr float FRONT_WALKING_FRAME_TIME = 0.08f;

    // Run
    constexpr int   FRONT_RUNNING_FRAMES   = 12;
    constexpr float FRONT_RUNNING_FRAME_TIME = 0.06f;

    // Attack
    constexpr int   ATTACK_FRAMES     = 10;
    constexpr float ATTACK_FRAME_TIME = 0.05f;

    // Hurt
    constexpr int   HURT_FRAMES     = 4;
    constexpr float HURT_FRAME_TIME = 0.1f;

    // Death
    constexpr int   DEATH_FRAMES     = 8;
    constexpr float DEATH_FRAME_TIME = 0.1f;
}

// ====================== ENEMY (ví dụ sau này) ======================
namespace DefEnemyAnim
{
    constexpr int   IDLE_FRAMES     = 6;
    constexpr float IDLE_FRAME_TIME = 0.12f;
}

