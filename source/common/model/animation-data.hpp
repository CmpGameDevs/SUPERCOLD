#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace our {

enum class InterpolationType { LINEAR, STEP, CUBICSPLINE };

template <typename T> struct Keyframe {
    float time;
    T value;
    // Optional tangents for cubic spline interpolation
    T inTangent;  // For cubic spline
    T outTangent; // For cubic spline
    InterpolationType interpolationType;
};

using TranslationKeyframe = Keyframe<glm::vec3>;
using RotationKeyframe = Keyframe<glm::quat>;
using ScaleKeyframe = Keyframe<glm::vec3>;

// A variant to hold different keyframe types
using KeyframeValue = std::variant<glm::vec3, glm::quat>;

struct Channel {
    enum class PathType { Translation, Rotation, Scale };

    PathType path;
    int nodeIndex;
    std::string name; // Added for debugging and visualization
    InterpolationType interpolationType;

    // Use specific keyframe types instead of variant
    std::vector<TranslationKeyframe> translationKeys;
    std::vector<RotationKeyframe> rotationKeys;
    std::vector<ScaleKeyframe> scaleKeys;

    // Cache for optimization
    float startTime = std::numeric_limits<float>::max();
    float endTime = std::numeric_limits<float>::min();
};

struct BoneInfo {
    int id;                    // Index in shader's bone matrix array
    glm::mat4 offsetMatrix;    // Inverse bind matrix
    glm::mat4 globalTransform; // Current global transform
    std::string name;          // Added for debugging
    bool isDirty = true;       // For transform update optimization
};

// Structure to hold skeletal hierarchy
struct BoneNode {
    std::string name;
    int index;
    glm::mat4 localTransform;
    glm::mat4 globalTransform; // Added for caching

    // Transform components for easier manipulation
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);

    BoneNode *parent = nullptr; // Added parent pointer
    std::vector<BoneNode *> children;

    // Added functionality
    bool isDirty = true;

    // Helper functions
    void updateLocalTransform() {
        if (isDirty) {
            glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);
            glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
            glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);
            localTransform = translationMatrix * rotationMatrix * scaleMatrix;
            isDirty = false;
        }
    }

    void updateGlobalTransform(const glm::mat4 &parentTransform = glm::mat4(1.0f)) {
        updateLocalTransform();
        globalTransform = parentTransform * localTransform;

        for (auto *child : children) {
            child->updateGlobalTransform(globalTransform);
        }
    }
};

// Data structure to hold all animation and skeletal data for a model
// Enhanced AnimationClip structure
struct AnimationClip {
    std::string name;
    float duration = 0.0f;
    float speed = 1.0f;    // Added playback speed control
    bool isLooping = true; // Added looping control
    std::vector<Channel> channels;

    // Added timing info
    float startTime = 0.0f;
    float endTime = 0.0f;

    // Added state tracking
    bool isPlaying = false;
    float currentTime = 0.0f;

    // Helper functions
    void updateTimingInfo() {
        startTime = std::numeric_limits<float>::max();
        endTime = std::numeric_limits<float>::min();

        for (const auto &channel : channels) {
            startTime = std::min(startTime, channel.startTime);
            endTime = std::max(endTime, channel.endTime);
        }
        duration = endTime - startTime;
    }
};

// Enhanced AnimationData structure
struct AnimationData {
    std::map<std::string, AnimationClip> animationClips;
    std::map<int, BoneInfo> boneInfoMap;
    std::map<std::string, BoneNode *> boneNameMap; // Added for quick name lookup
    int boneCounter = 0;
    BoneNode *rootBone = nullptr;

    // Added animation state
    std::string currentAnimation;
    float animationTime = 0.0f;
    bool isPlaying = false;

    // Added bone transform cache
    std::vector<glm::mat4> globalBoneTransforms;

    // Helper functions
    BoneNode *findBoneByName(const std::string &name) {
        auto it = boneNameMap.find(name);
        return it != boneNameMap.end() ? it->second : nullptr;
    }

    void updateGlobalTransforms() {
        if (rootBone) {
            rootBone->updateGlobalTransform();
        }
    }
};

} // namespace our
