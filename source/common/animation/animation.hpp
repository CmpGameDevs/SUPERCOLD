#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

struct KeyFrame {
    float timeStamp;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;
};

struct BoneAnimation {
    std::string boneName;
    std::vector<KeyFrame> keyFrames;
};

class Animation {
public:
    std::string name;
    float duration;
    float ticksPerSecond;
    std::vector<BoneAnimation> boneAnimations;

    Animation(): name(""), duration(0.0f), ticksPerSecond(0.0f) {}
};
