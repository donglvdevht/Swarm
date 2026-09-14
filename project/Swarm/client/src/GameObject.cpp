#include "GameObject.h"
#include "AnimationData.h"

GameObject::GameObject()
    : currentAnimation(-1),
      position(0.0f, 0.0f),
      type(ObjectType::player),
    facingDirection(Direction::Down),
    attacking(false),
    attackTimeRemaining(0.0f)
{
}

void GameObject::attack()
{
    if (!attacking) {
        attacking = true;
        attackTimeRemaining = DefPlayerAnim::ATTACK_FRAMES *
                              DefPlayerAnim::ATTACK_FRAME_TIME;
    }
}

void GameObject::updateAttack(float deltaTime)
{
    if (!attacking) {
        return;
    }

    attackTimeRemaining -= deltaTime;
    if (attackTimeRemaining <= 0.0f) {
        attackTimeRemaining = 0.0f;
        attacking = false;
    }
}

void GameObject::move(const glm::vec2& direction, float deltaTime, bool running)
{
    if (glm::length(direction) < 0.0001f) {
        return;
    }

    const float speed = running ? RUN_SPEED : WALK_SPEED;
    position += direction * speed * deltaTime;

    if (std::abs(direction.x) > std::abs(direction.y)) {
        facingDirection = direction.x > 0.0f ? Direction::Right : Direction::Left;
    } else {
        facingDirection = direction.y > 0.0f ? Direction::Down : Direction::Up;
    }
}