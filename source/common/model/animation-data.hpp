#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace our {

struct TranslationKeyframe {
    float time;
    glm::vec3 value;
};

struct RotationKeyframe {
    float time;
    glm::quat value;
};

struct ScaleKeyframe {
    float time;
    glm::vec3 value;
};

// A variant to hold different keyframe types
using KeyframeValue = std::variant<glm::vec3, glm::quat>;

struct Channel {
    enum class PathType { Translation, Rotation, Scale };
    PathType path;
    int nodeIndex; // The index of the node (bone) being animated
    std::vector<float> keyframeTimes;
    std::vector<KeyframeValue> keyframeValues; // Store keyframe values directly
    std::string interpolationType;                     
};

struct AnimationClip {
    std::string name;
    float duration = 0.0f; // Calculated from keyframe times
    std::vector<Channel> channels;
};

struct BoneInfo {
    int id;                 // Index in the shader's bone matrix array
    glm::mat4 offsetMatrix; // The inverse bind matrix
};

// Structure to hold skeletal hierarchy
struct BoneNode {
    std::string name;
    int index;                // Index in the bone array
    glm::mat4 localTransform; // Initial local transform
    std::vector<BoneNode *> children;

    // Destructor to clean up children
    ~BoneNode() {
        for (auto child : children) {
            delete child;
        }
    }
};

// Data structure to hold all animation and skeletal data for a model
struct AnimationData {
    std::map<std::string, AnimationClip> animationClips;
    std::map<int, BoneInfo> boneInfoMap; // Maps glTF node index to BoneInfo
    int boneCounter = 0;                 // To assign unique IDs to bones
    BoneNode *rootBone = nullptr;        // The root of the bone hierarchy

    // Destructor
    ~AnimationData() {
        if (rootBone)
            delete rootBone;
    }
};

} // namespace our
