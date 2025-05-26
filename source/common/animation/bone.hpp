#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

struct Bone {
    std::string name;
    int id;
    glm::mat4 localBindTransform; // Bone's transform in bind pose
    glm::mat4 offsetMatrix;       // Inverse bind pose matrix
    int parentIndex;              // -1 for root bones
    std::vector<int> children;    // Child bone indices
};
