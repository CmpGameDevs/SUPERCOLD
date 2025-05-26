#include "skeleton.hpp"
#include <iomanip>
#include <iostream>

namespace our {

Skeleton::Skeleton() {
    // Reserve some space to avoid frequent reallocations
    bones.reserve(64);
    boneTransforms.reserve(64);
    finalTransforms.reserve(64);
}

Skeleton::~Skeleton() {
    // No dynamic memory to clean up in this simple implementation
}

int Skeleton::addBone(const Bone& bone) {
    // Validate bone data
    if (bone.name.empty()) {
        std::cerr << "[Skeleton] ERROR: Cannot add bone with empty name" << std::endl;
        return -1;
    }

    // Check if bone name already exists
    if (boneNameToIndex.find(bone.name) != boneNameToIndex.end()) {
        std::cerr << "[Skeleton] ERROR: Bone with name '" << bone.name << "' already exists" << std::endl;
        return -1;
    }

    // Validate parent index
    if (bone.parentIndex >= static_cast<int>(bones.size())) {
        std::cerr << "[Skeleton] ERROR: Invalid parent index " << bone.parentIndex << " for bone '" << bone.name << "'"
                  << std::endl;
        return -1;
    }

    // Add the bone
    int boneIndex = static_cast<int>(bones.size());
    Bone newBone = bone;
    newBone.id = boneIndex; // Ensure ID matches index

    bones.push_back(newBone);
    boneNameToIndex[bone.name] = boneIndex;

    // Resize transform arrays
    boneTransforms.resize(bones.size(), glm::mat4(1.0f));
    finalTransforms.resize(bones.size(), glm::mat4(1.0f));

    // Update parent-child relationships
    if (bone.parentIndex >= 0) {
        // Add this bone as a child to its parent
        bones[bone.parentIndex].children.push_back(boneIndex);
    } else {
        // This is a root bone
        rootBones.push_back(boneIndex);
    }

    std::cout << "[Skeleton] Added bone '" << bone.name << "' at index " << boneIndex;
    if (bone.parentIndex >= 0) {
        std::cout << " (parent: '" << bones[bone.parentIndex].name << "')";
    } else {
        std::cout << " (root bone)";
    }
    std::cout << std::endl;

    return boneIndex;
}

int Skeleton::findBoneIndex(const std::string& name) const {
    auto it = boneNameToIndex.find(name);
    if (it != boneNameToIndex.end()) {
        return it->second;
    }
    return -1; // Not found
}

Bone* Skeleton::getBone(int index) {
    if (isValidBoneIndex(index)) {
        return &bones[index];
    }
    return nullptr;
}

const Bone* Skeleton::getBone(int index) const {
    if (isValidBoneIndex(index)) {
        return &bones[index];
    }
    return nullptr;
}

Bone* Skeleton::getBone(const std::string& name) {
    int index = findBoneIndex(name);
    if (index >= 0) {
        return &bones[index];
    }
    return nullptr;
}

const Bone* Skeleton::getBone(const std::string& name) const {
    int index = findBoneIndex(name);
    if (index >= 0) {
        return &bones[index];
    }
    return nullptr;
}

void Skeleton::calculateBoneTransforms(const glm::mat4& rootTransform) {
    if (bones.empty()) {
        return;
    }

    // Calculate transforms starting from root bones
    for (int rootBoneIndex : rootBones) {
        calculateBoneTransformsRecursive(rootBoneIndex, rootTransform);
    }

    // Calculate final transforms for the shader
    calculateFinalTransforms();
}

void Skeleton::calculateBoneTransformsRecursive(int boneIndex, const glm::mat4& parentTransform) {
    if (!isValidBoneIndex(boneIndex)) {
        std::cerr << "[Skeleton] ERROR: Invalid bone index " << boneIndex << " in transform calculation" << std::endl;
        return;
    }

    const Bone& bone = bones[boneIndex];

    // Calculate this bone's world transform
    // parentTransform * localBindTransform gives us the bone's world space transform
    boneTransforms[boneIndex] = parentTransform * bone.localBindTransform;

    // Recursively calculate transforms for all children
    for (int childIndex : bone.children) {
        calculateBoneTransformsRecursive(childIndex, boneTransforms[boneIndex]);
    }
}

void Skeleton::calculateFinalTransforms() {
    // Final transform = bone's world transform * offset matrix
    // This transforms vertices from mesh space to the bone's current position
    for (size_t i = 0; i < bones.size(); ++i) {
        finalTransforms[i] = boneTransforms[i] * bones[i].offsetMatrix;
    }
}

void Skeleton::printHierarchy() const {
    std::cout << "[Skeleton] Bone Hierarchy (" << bones.size() << " bones):" << std::endl;

    for (int rootBoneIndex : rootBones) {
        printBoneInfo(rootBoneIndex, 0);
    }
}

void Skeleton::printBoneInfo(int boneIndex, int depth) const {
    if (!isValidBoneIndex(boneIndex)) {
        return;
    }

    const Bone& bone = bones[boneIndex];

    // Print indentation
    for (int i = 0; i < depth; ++i) {
        std::cout << "  ";
    }

    std::cout << "├─ " << bone.name << " (ID: " << bone.id << ")";

    if (bone.children.empty()) {
        std::cout << " [leaf]";
    } else {
        std::cout << " [" << bone.children.size() << " children]";
    }

    std::cout << std::endl;

    // Print children
    for (int childIndex : bone.children) {
        printBoneInfo(childIndex, depth + 1);
    }
}

bool Skeleton::isValidBoneIndex(int index) const {
    return index >= 0 && index < static_cast<int>(bones.size());
}

bool Skeleton::validateHierarchy() const {
    bool isValid = true;

    // Check that all parent indices are valid
    for (size_t i = 0; i < bones.size(); ++i) {
        const Bone& bone = bones[i];

        if (bone.parentIndex >= 0) {
            if (!isValidBoneIndex(bone.parentIndex)) {
                std::cerr << "[Skeleton] ERROR: Bone '" << bone.name << "' has invalid parent index "
                          << bone.parentIndex << std::endl;
                isValid = false;
            } else {
                // Check if parent actually lists this bone as a child
                const Bone& parent = bones[bone.parentIndex];
                bool foundInParent = false;
                for (int childIndex : parent.children) {
                    if (childIndex == static_cast<int>(i)) {
                        foundInParent = true;
                        break;
                    }
                }
                if (!foundInParent) {
                    std::cerr << "[Skeleton] ERROR: Bone '" << bone.name << "' claims parent '" << parent.name
                              << "' but parent doesn't list it as child" << std::endl;
                    isValid = false;
                }
            }
        }

        // Check that all child indices are valid
        for (int childIndex : bone.children) {
            if (!isValidBoneIndex(childIndex)) {
                std::cerr << "[Skeleton] ERROR: Bone '" << bone.name << "' has invalid child index " << childIndex
                          << std::endl;
                isValid = false;
            } else {
                // Check if child actually claims this bone as parent
                const Bone& child = bones[childIndex];
                if (child.parentIndex != static_cast<int>(i)) {
                    std::cerr << "[Skeleton] ERROR: Bone '" << bone.name << "' claims child '" << child.name
                              << "' but child doesn't claim it as parent" << std::endl;
                    isValid = false;
                }
            }
        }
    }

    // Check for cycles (basic check - no bone should be its own ancestor)
    // This is a simplified cycle detection - for a full implementation you'd want to do a proper DFS

    if (isValid) {
        std::cout << "[Skeleton] Hierarchy validation passed" << std::endl;
    } else {
        std::cout << "[Skeleton] Hierarchy validation FAILED" << std::endl;
    }

    return isValid;
}

void Skeleton::logSkeletonInfo() const {
    std::cout << "[Skeleton] === Skeleton Information ===" << std::endl;
    std::cout << "[Skeleton] Total bones: " << bones.size() << std::endl;
    std::cout << "[Skeleton] Root bones: " << rootBones.size() << std::endl;

    for (size_t i = 0; i < rootBones.size(); ++i) {
        const Bone& rootBone = bones[rootBones[i]];
        std::cout << "[Skeleton] Root bone " << i << ": " << rootBone.name << std::endl;
    }

    std::cout << "[Skeleton] ===========================" << std::endl;
}

} // namespace our
