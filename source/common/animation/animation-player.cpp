#include "animation-player.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include "glm/gtx/quaternion.hpp"

namespace our {

AnimationPlayer::AnimationPlayer()
    : m_CurrentAnimation(nullptr), m_CurrentTimeSeconds(0.0f), m_IsPlaying(false), m_IsLooping(false) {}

void AnimationPlayer::playAnimation(Animation* animation, bool loop) {
    m_CurrentAnimation = animation;
    m_CurrentTimeSeconds = 0.0f;
    m_IsPlaying = (m_CurrentAnimation != nullptr);
    m_IsLooping = loop;

    if (m_CurrentAnimation && m_IsPlaying) {
        std::cout << "[AnimationPlayer] Playing animation: '" << m_CurrentAnimation->name
                  << "'. Loop: " << (m_IsLooping ? "Yes" : "No") << std::endl;
    } else if (!m_CurrentAnimation) {
        std::cout << "[AnimationPlayer] Attempted to play a null animation. Playback not started." << std::endl;
    }
}

void AnimationPlayer::update(float deltaTimeInSeconds) {
    if (!m_IsPlaying || !m_CurrentAnimation) {
        return;
    }

    float ticksPerSecond = m_CurrentAnimation->ticksPerSecond;
    if (ticksPerSecond <= 0.0f) {
        ticksPerSecond = 25.0f;
    }

    float durationInTicks = m_CurrentAnimation->duration;
    float durationInSeconds = durationInTicks / ticksPerSecond;

    if (durationInSeconds <= 0.00001f) {
        m_CurrentTimeSeconds = 0.0f;
        m_IsPlaying = false;
        // std::cout << "[AnimationPlayer] Animation '" << m_CurrentAnimation->name << "' has zero or negative duration.
        // Stopped." << std::endl;
        return;
    }

    m_CurrentTimeSeconds += deltaTimeInSeconds;

    if (m_CurrentTimeSeconds >= durationInSeconds) {
        if (m_IsLooping) {
            // Use fmod for precise looping, handles multiple loops if deltaTime is large
            m_CurrentTimeSeconds = fmod(m_CurrentTimeSeconds, durationInSeconds);
            if (m_CurrentTimeSeconds < 0)
                m_CurrentTimeSeconds += durationInSeconds;

        } else {
            m_CurrentTimeSeconds = durationInSeconds;
            m_IsPlaying = false;
            std::cout << "[AnimationPlayer] Animation finished: '" << m_CurrentAnimation->name << "'" << std::endl;
        }
    } else if (m_CurrentTimeSeconds < 0.0f) {
        if (m_IsLooping) {
            m_CurrentTimeSeconds = fmod(m_CurrentTimeSeconds, durationInSeconds);
            if (m_CurrentTimeSeconds < 0)
                m_CurrentTimeSeconds += durationInSeconds;
        } else {
            m_CurrentTimeSeconds = 0.0f;
        }
    }
}

void AnimationPlayer::pause() {
    if (m_IsPlaying) {
        m_IsPlaying = false;
        // std::cout << "[AnimationPlayer] Paused." << std::endl;
    }
}

void AnimationPlayer::resume() {
    if (!m_IsPlaying && m_CurrentAnimation) {
        m_IsPlaying = true;
        // std::cout << "[AnimationPlayer] Resumed." << std::endl;
    }
}

void AnimationPlayer::stop() {
    m_IsPlaying = false;
    m_CurrentTimeSeconds = 0.0f;
    // std::cout << "[AnimationPlayer] Stopped." << std::endl;
}

void AnimationPlayer::setTime(float timeInSeconds) {
    m_CurrentTimeSeconds = timeInSeconds;
    if (m_CurrentAnimation) {
        float ticksPerSecond = m_CurrentAnimation->ticksPerSecond > 0.0f ? m_CurrentAnimation->ticksPerSecond : 25.0f;
        float durationInSeconds = m_CurrentAnimation->duration / ticksPerSecond;

        if (durationInSeconds <= 0.00001f) {
            m_CurrentTimeSeconds = 0.0f;
            return;
        }

        if (m_IsLooping) {
            m_CurrentTimeSeconds = fmod(m_CurrentTimeSeconds, durationInSeconds);
            if (m_CurrentTimeSeconds < 0.0f) {
                m_CurrentTimeSeconds += durationInSeconds;
            }
        } else {
            m_CurrentTimeSeconds = std::max(0.0f, std::min(m_CurrentTimeSeconds, durationInSeconds));
        }
    } else {
        m_CurrentTimeSeconds = 0.0f;
    }
}

const BoneAnimation* AnimationPlayer::findBoneAnimationTrack(const std::string& boneName) const {
    if (!m_CurrentAnimation) {
        return nullptr;
    }
    for (const auto& boneAnimTrack : m_CurrentAnimation->boneAnimations) {
        if (boneAnimTrack.boneName == boneName) {
            return &boneAnimTrack;
        }
    }
    return nullptr;
}

int AnimationPlayer::getPreviousKeyFrameIndex(const std::vector<KeyFrame>& keyFrames, float timeInAnimationTicks) {
    if (keyFrames.empty()) {
        return -1;
    }
    if (keyFrames.size() == 1 || timeInAnimationTicks <= keyFrames[0].timeStamp) {
        return 0;
    }
    if (timeInAnimationTicks >= keyFrames.back().timeStamp) {
        return static_cast<int>(keyFrames.size() - 1); // Use the last keyframe.
    }

    // Binary search could be an optimization here if keyFrames are numerous and sorted.
    // For now, linear scan is fine and matches typical Assimp loader behavior.
    for (size_t i = 0; i < keyFrames.size() - 1; ++i) {
        if (timeInAnimationTicks < keyFrames[i + 1].timeStamp) {
            return static_cast<int>(i);
        }
    }

    return static_cast<int>(keyFrames.size() - 1);
}

KeyFrame AnimationPlayer::interpolateKeyFrames(const std::vector<KeyFrame>& keyFrames, float timeInAnimationTicks) {
    if (keyFrames.empty()) {
        // Return a default/identity KeyFrame if there are no keyframes for this bone track
        return {timeInAnimationTicks, glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f)};
    }

    int p0_Index = getPreviousKeyFrameIndex(keyFrames, timeInAnimationTicks);

    if (p0_Index == 0 && (timeInAnimationTicks <= keyFrames[0].timeStamp || keyFrames.size() == 1)) {
        return keyFrames[0];
    }
    if (p0_Index == static_cast<int>(keyFrames.size()) - 1) {
        return keyFrames.back();
    }

    int p1_Index = p0_Index + 1;
    if (p1_Index >= static_cast<int>(keyFrames.size())) {
        return keyFrames[p0_Index];
    }

    const KeyFrame& p0 = keyFrames[p0_Index];
    const KeyFrame& p1 = keyFrames[p1_Index];

    float deltaTimeBetweenKeys = p1.timeStamp - p0.timeStamp;
    float factor = 0.0f;

    if (deltaTimeBetweenKeys > 0.00001f) {
        factor = (timeInAnimationTicks - p0.timeStamp) / deltaTimeBetweenKeys;
    }
    // Clamp factor to [0, 1] to prevent extrapolation if time is slightly outside due to precision
    factor = glm::clamp(factor, 0.0f, 1.0f);

    KeyFrame interpolatedFrame;
    interpolatedFrame.timeStamp = timeInAnimationTicks;
    interpolatedFrame.position = glm::mix(p0.position, p1.position, factor);
    interpolatedFrame.rotation = glm::slerp(p0.rotation, p1.rotation, factor);
    interpolatedFrame.scale = glm::mix(p0.scale, p1.scale, factor);

    return interpolatedFrame;
}

glm::mat4 AnimationPlayer::getBoneTransformAtArbitraryTime(const std::string& boneName,
                                                           float timeInAnimationTicks) const {
    const BoneAnimation* boneAnimTrack = findBoneAnimationTrack(boneName);

    if (!boneAnimTrack) {
        return glm::mat4(1.0f);
    }

    KeyFrame interpolatedFrame = interpolateKeyFrames(boneAnimTrack->keyFrames, timeInAnimationTicks);

    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), interpolatedFrame.position);
    glm::mat4 rotationMatrix = glm::toMat4(interpolatedFrame.rotation); // Convert quaternion to rotation matrix
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), interpolatedFrame.scale);

    return translationMatrix * rotationMatrix * scaleMatrix;
}

glm::mat4 AnimationPlayer::getCurrentBoneTransform(const std::string& boneName) const {
    if (!m_CurrentAnimation) {
        return glm::mat4(1.0f);
    }

    float ticksPerSecond = m_CurrentAnimation->ticksPerSecond;
    if (ticksPerSecond <= 0.0f)
        ticksPerSecond = 25.0f;

    float timeInAnimationTicks = m_CurrentTimeSeconds * ticksPerSecond;

    if (m_IsLooping && m_CurrentAnimation->duration > 0.00001f) {
        timeInAnimationTicks = fmod(timeInAnimationTicks, m_CurrentAnimation->duration);
        if (timeInAnimationTicks < 0.0f) {
            timeInAnimationTicks += m_CurrentAnimation->duration;
        }
    }

    return getBoneTransformAtArbitraryTime(boneName, timeInAnimationTicks);
}

} // namespace our
