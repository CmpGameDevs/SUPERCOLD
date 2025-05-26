#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "animation/animation.hpp"

namespace our {

class AnimationPlayer {
    public:
    AnimationPlayer();

    void playAnimation(Animation* animation, bool loop = true);

    void update(float deltaTimeInSeconds);

    void pause();
    void resume();
    void stop();

    void setTime(float timeInSeconds);

    bool isCurrentlyPlaying() const {
        return m_IsPlaying;
    }
    float getCurrentTimeSeconds() const {
        return m_CurrentTimeSeconds;
    }
    Animation* getCurrentAnimation() const {
        return m_CurrentAnimation;
    }

    glm::mat4 getCurrentBoneTransform(const std::string& boneName) const;

    static KeyFrame interpolateKeyFrames(const std::vector<KeyFrame>& keyFrames, float timeInAnimationTicks);

    glm::mat4 getBoneTransformAtArbitraryTime(const std::string& boneName, float timeInAnimationTicks) const;

    private:
    Animation* m_CurrentAnimation;
    float m_CurrentTimeSeconds;
    bool m_IsPlaying;
    bool m_IsLooping;

    const BoneAnimation* findBoneAnimationTrack(const std::string& boneName) const;

    static int getPreviousKeyFrameIndex(const std::vector<KeyFrame>& keyFrames, float timeInAnimationTicks);
};

} // namespace our
