#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include "bone.hpp"


namespace our {

class Skeleton {
    private:
    std::vector<Bone> bones;                              // All bones in the skeleton
    std::unordered_map<std::string, int> boneNameToIndex; // Fast lookup: bone name -> bone index
    std::vector<glm::mat4> boneTransforms;                // Current pose transforms (bone space to world space)
    std::vector<glm::mat4> finalTransforms;               // Final matrices sent to shader (mesh space to bone space)

    // Root bone indices (bones with no parent)
    std::vector<int> rootBones;

    public:
    Skeleton();
    ~Skeleton();

    // Bone management
    int addBone(const Bone& bone);
    int findBoneIndex(const std::string& name) const;
    Bone* getBone(int index);
    const Bone* getBone(int index) const;
    Bone* getBone(const std::string& name);
    const Bone* getBone(const std::string& name) const;

    // Getters
    size_t getBoneCount() const {
        return bones.size();
    }
    const std::vector<Bone>& getBones() const {
        return bones;
    }
    const std::vector<glm::mat4>& getBoneTransforms() const {
        return boneTransforms;
    }
    const std::vector<glm::mat4>& getFinalTransforms() const {
        return finalTransforms;
    }
    const std::vector<int>& getRootBones() const {
        return rootBones;
    }

    // Transform calculations
    void calculateBoneTransforms(const glm::mat4& rootTransform = glm::mat4(1.0f));
    void calculateBoneTransformsRecursive(int boneIndex, const glm::mat4& parentTransform);
    void calculateFinalTransforms();

    // Utility functions
    void printHierarchy() const;
    void printBoneInfo(int boneIndex, int depth = 0) const;
    bool isValidBoneIndex(int index) const;

    // Debug/validation
    bool validateHierarchy() const;
    void logSkeletonInfo() const;
};

} // namespace our
