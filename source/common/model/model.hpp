#pragma once

#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <components/camera.hpp>
#include <components/mesh-renderer.hpp>
#include <map>
#include <material/material.hpp>
#include <memory>
#include <mesh/mesh.hpp>
#include <set>
#include <string>
#include <vector>
#include "animation/animation.hpp"
#include "animation/skeleton.hpp"
#include "texture/texture2d.hpp"

namespace our {

class Model {
    public:
    Skeleton skeleton;
    std::vector<Animation> animations; // List of animations associated with this model

    Model() = default;
    ~Model();

    // Load a model file (fbx, obj, gltf, etc.) using Assimp
    bool loadFromFile(const std::string& path);

    // Draw all meshes in the model
    void draw(CameraComponent* camera, const glm::mat4& localToWorld, const glm::ivec2& windowSize,
              float bloomCutoff) const;

    // Generate a single combined mesh for all submeshes
    void generateCombinedMesh();

    // Access the combined mesh for collision or other purposes
    Mesh* getCombinedMesh() const {
        return combinedMesh.get();
    }

    private:
    std::string directory;
    std::vector<MeshRendererComponent*> meshRenderers;
    std::unique_ptr<Mesh> combinedMesh;

    // Materials and textures owned by this model.
    std::vector<std::unique_ptr<Material>> materials;
    std::map<std::string, std::shared_ptr<Texture2D>> texture_cache; // Key: relative path from model fill

    // Assimp import methods
    void processNode(const aiNode* node, const aiScene* scene, const glm::mat4& parentTransform);
    MeshRendererComponent* processMesh(const aiMesh* mesh, const aiScene* scene, const glm::mat4& transform);

    // Material and Texture loading helpers
    void loadMaterialsFromScene(const aiScene* scene);
    std::unique_ptr<Material> processMaterial(const aiMaterial* aiMat, const aiScene* scene);

    // Loads texture, uses mTextureCache. `texturePathInModel` is relative path as
    // stored in model file.
    std::shared_ptr<Texture2D> loadTexture(const std::string& texturePathInModel);
    // Gets texture path from aiMaterial and then calls loadTexture.
    std::shared_ptr<Texture2D> loadMaterialTexture(const aiMaterial* aiMat, const aiScene* scene, aiTextureType type,
                                                   unsigned int index = 0);

    // Skeleton
    void loadSkeletonFromScene(const aiScene* scene);
    void processNodeForSkeleton(const aiNode* assimpNode, int parentBoneIndexInSkeleton,
                                const std::map<std::string, glm::mat4>& boneOffsetMatrices,
                                const std::set<std::string>& boneNamesFromMeshes);
    void processVertexBoneData(const aiMesh* mesh, std::vector<Vertex>& vertices);

    // Animation
    void loadAnimationsFromScene(const aiScene* scene);
    template <typename TKey> static int findKeyFrameIndex(float animationTime, const TKey* keys, unsigned int numKeys);

    // Helpers
    public:
    static glm::mat4 aiToGlm(const aiMatrix4x4& m);
    static glm::vec3 aiToGlm(const aiVector3D& v) {
        return {v.x, v.y, v.z};
    }
    static glm::quat aiToGlm(const aiQuaternion& q) {
        return glm::quat(q.w, q.x, q.y, q.z);
    }
    static glm::vec3 aiToGlm(const aiColor3D& c) {
        return {c.r, c.g, c.b};
    }
    static glm::vec4 aiToGlm(const aiColor4D& c) {
        return {c.r, c.g, c.b, c.a};
    }
    static std::string aiToStr(const aiString& s) {
        return s.C_Str();
    }
};

} // namespace our
