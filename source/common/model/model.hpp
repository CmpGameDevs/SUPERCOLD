#pragma once

#include "texture/texture2d.hpp"
#include <assimp/scene.h>
#include <components/camera.hpp>
#include <components/mesh-renderer.hpp>
#include <glm/glm.hpp>
#include <map>
#include <material/material.hpp>
#include <memory>
#include <mesh/mesh.hpp>
#include <string>
#include <vector>

namespace our {

class Model {
public:
  Model() = default;
  ~Model();

  // Load a model file (fbx, obj, gltf, etc.) using Assimp
  bool loadFromFile(const std::string &path);

  // Draw all meshes in the model
  void draw(CameraComponent *camera, const glm::mat4 &localToWorld,
            const glm::ivec2 &windowSize, float bloomCutoff) const;

  // Generate a single combined mesh for all submeshes
  void generateCombinedMesh();

  // Access the combined mesh for collision or other purposes
  Mesh *getCombinedMesh() const { return combinedMesh.get(); }

private:
  std::string directory;
  std::vector<MeshRendererComponent *> meshRenderers;
  std::unique_ptr<Mesh> combinedMesh;

  // Materials and textures owned by this model.
  std::vector<std::unique_ptr<Material>> materials;
  std::map<std::string, std::shared_ptr<Texture2D>>
      texture_cache; // Key: relative path from model fill

  // Assimp import methods
  void processNode(const aiNode *node, const aiScene *scene,
                   const glm::mat4 &parentTransform);
  MeshRendererComponent *processMesh(const aiMesh *mesh, const aiScene *scene,
                                     const glm::mat4 &transform);

  // Material and Texture loading helpers
  void loadMaterialsFromScene(const aiScene *scene);
  std::unique_ptr<Material> processMaterial(const aiMaterial *aiMat,
                                            const aiScene *scene);

  // Loads texture, uses mTextureCache. `texturePathInModel` is relative path as
  // stored in model file.
  std::shared_ptr<Texture2D> loadTexture(const std::string &texturePathInModel);
  // Gets texture path from aiMaterial and then calls loadTexture.
  std::shared_ptr<Texture2D> loadMaterialTexture(const aiMaterial *aiMat,
                                                 const aiScene *scene,
                                                 aiTextureType type,
                                                 unsigned int index = 0);

  // Helpers
  static glm::mat4 aiToGlm(const aiMatrix4x4 &m);
  static glm::vec3 aiToGlm(const aiColor3D &c) { return {c.r, c.g, c.b}; }
  static glm::vec4 aiToGlm(const aiColor4D &c) { return {c.r, c.g, c.b, c.a}; }
  static std::string aiToStr(const aiString &s) { return s.C_Str(); }
};

} // namespace our
