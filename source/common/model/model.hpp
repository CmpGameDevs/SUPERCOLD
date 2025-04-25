#include <model/animation-data.hpp>
#include <components/camera.hpp>
#include <components/mesh-renderer.hpp>
#include <ecs/entity.hpp>
#include <fstream>
#include <iostream>
#include <json/json.hpp>
#include <material/material.hpp>
#include <mesh/mesh.hpp>
#include <string>
#include <texture/texture-utils.hpp>
#include <texture/texture2d.hpp>
#include <vector>

using json = nlohmann::json;

namespace our {
class Model {

    std::vector<Texture2D *> textures;
    std::vector<Material *> materials;
    std::vector<MeshRendererComponent *> meshRenderers;
    std::vector<glm::vec3> translationsMeshes;
    std::vector<glm::quat> rotationsMeshes;
    std::vector<glm::vec3> scalesMeshes;
    std::vector<glm::mat4> matricesMeshes;
    std::vector<unsigned char> data;
    json JSON;
    std::string path;
    AnimationData animationData;

  public:
    ~Model() {
        for (auto &texture : textures)
            delete texture;
        for (auto &material : materials)
            delete material;
        for (auto &mesh : meshRenderers)
            delete mesh;
    }

    void draw(CameraComponent *camera, glm::mat4 localToWorld, glm::ivec2 windowSize, float bloomBrightnessCutoff);
    void loadTextures();
    void loadMesh(unsigned int indMesh);
    void loadMaterials();
    void traverseNode(unsigned int nextNode, glm::mat4 matrix = glm::mat4(1.0f));
    void loadModel(std::string path);
    void loadAnimations(); 

    // Reads a text file and outputs a string with everything in the text file
    std::string get_file_contents(std::string path);
    std::vector<unsigned char> get_file_binary_contents(const std::string &path);
    std::vector<unsigned char> getData();

    // Interprets the binary data into floats, indices, and textures
    std::vector<float> getFloats(json accessor);
    std::vector<short> getShorts(json accessor);
    std::vector<GLuint> getIndices(json accessor);

    // Assembles all the floats into vertices
    std::vector<Vertex> assembleVertices(std::vector<glm::vec3> positions, std::vector<glm::vec3> normals,
                                    std::vector<glm::vec2> texUVs, 
                                    std::vector<glm::ivec4> boneIndices = std::vector<glm::ivec4>(), 
                                    std::vector<glm::vec4> boneWeights = std::vector<glm::vec4>());

    // Helps with the assembly from above by grouping floats
    std::vector<glm::vec2> groupFloatsVec2(std::vector<float> floatVec);
    std::vector<glm::vec3> groupFloatsVec3(std::vector<float> floatVec);
    std::vector<glm::vec4> groupFloatsVec4(std::vector<float> floatVec);
};
} // namespace our