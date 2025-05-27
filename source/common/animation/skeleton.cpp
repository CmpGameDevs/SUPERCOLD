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
    if (boneNameToIndex.find(bone.name) != boneNameToIndex.end()) {
        std::cerr << "[Skeleton] ERROR: Bone with name '" << bone.name << "' already exists" << std::endl;
        return -1;
    }
    if (bone.parentIndex >= static_cast<int>(bones.size())) {
        std::cerr << "[Skeleton] ERROR: Invalid parent index " << bone.parentIndex << " for bone '" << bone.name << "'"
                  << std::endl;
        return -1;
    }

    int boneIndex = static_cast<int>(bones.size());
    Bone newBone = bone;
    newBone.id = boneIndex;

    bones.push_back(newBone);
    boneNameToIndex[bone.name] = boneIndex;

    // Resize transform arrays when a new bone is added.
    // Initialize with identity matrix.
    boneTransforms.resize(bones.size(), glm::mat4(1.0f));
    finalTransforms.resize(bones.size(), glm::mat4(1.0f));

    if (bone.parentIndex >= 0) {
        if (isValidBoneIndex(bone.parentIndex)) { // Additional check
            bones[bone.parentIndex].children.push_back(boneIndex);
        } else {
            // This case should be caught by the earlier parentIndex validation, but good to be defensive
            std::cerr << "[Skeleton] ERROR: Parent index " << bone.parentIndex << " for bone '" << bone.name
                      << "' is out of bounds for parent's children list update." << std::endl;
            // Potentially revert the addBone operation or handle error
        }
    } else {
        rootBones.push_back(boneIndex);
    }
    // std::cout << ... (your existing logging)
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
    for (int rootBoneIndex : rootBones) {
        calculateBoneTransformsRecursive(rootBoneIndex, rootTransform);
    }
    calculateFinalTransforms();
}

void Skeleton::calculateBoneTransformsRecursive(int boneIndex, const glm::mat4& parentTransform) {
    if (!isValidBoneIndex(boneIndex)) {
        std::cerr << "[Skeleton] ERROR: Invalid bone index " << boneIndex << " in BIND POSE transform calculation"
                  << std::endl;
        return;
    }
    const Bone& bone = bones[boneIndex];
    boneTransforms[boneIndex] = parentTransform * bone.localBindTransform;

    for (int childIndex : bone.children) {
        calculateBoneTransformsRecursive(childIndex, boneTransforms[boneIndex]);
    }
}

void Skeleton::calculateAnimatedPoseRecursive(AnimationPlayer* player, int boneIndex,
                                              const glm::mat4& parentTransform) {
    if (!isValidBoneIndex(boneIndex) || !player) {
        std::cerr << "[Skeleton] ERROR: Invalid bone index " << boneIndex
                  << " or null player in ANIMATED POSE transform calculation" << std::endl;
        return;
    }
    const Bone& bone = bones[boneIndex];

    glm::mat4 localAnimatedTransform = player->getCurrentBoneTransform(bone.name);

    boneTransforms[boneIndex] = parentTransform * localAnimatedTransform;

    for (int childIndex : bone.children) {
        calculateAnimatedPoseRecursive(player, childIndex, boneTransforms[boneIndex]);
    }
}

void Skeleton::calculateAnimatedPose(AnimationPlayer* player, const glm::mat4& rootTransform) {
    if (bones.empty()) {
        return;
    }
    if (!player || !player->isCurrentlyPlaying() || !player->getCurrentAnimation()) {
        calculateBoneTransforms(rootTransform);
        return;
    }

    for (int rootBoneIndex : rootBones) {
        calculateAnimatedPoseRecursive(player, rootBoneIndex, rootTransform);
    }
    calculateFinalTransforms();
}

void Skeleton::calculateFinalTransforms() {
    if (boneTransforms.size() != bones.size() || finalTransforms.size() != bones.size()) {
        boneTransforms.resize(bones.size(), glm::mat4(1.0f));
        finalTransforms.resize(bones.size(), glm::mat4(1.0f));
    }
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
