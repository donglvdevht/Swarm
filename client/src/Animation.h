#pragma once
class Animation
{
    int frameCount;
    int currentFrame;
    float frameTime, timer;
public:
    Animation() 
            : frameCount(1)
            , currentFrame(0)
            , frameTime(0.1f)
            , timer(0.0f)
        {}

    Animation(float frameTime, int frameCount) 
        : frameTime(frameTime), frameCount(frameCount)
        , currentFrame(0), timer(0.0f) {}

    void update(float deltaTime) {
        if (frameCount <= 0 || frameTime <= 0.0f) return;
        timer += deltaTime;
        while (timer >= frameTime) {
            timer -= frameTime;
            currentFrame = (currentFrame + 1) % frameCount;
        }
    }

    int getCurrentFrame() const
        {
            return currentFrame;
        }

    void reset(){
        currentFrame = 0;
        timer = 0;
    }
};
