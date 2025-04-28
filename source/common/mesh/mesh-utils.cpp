#include "mesh-utils.hpp"

// We will use "Tiny OBJ Loader" to read and process '.obj" files
#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobj/tiny_obj_loader.h>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tinygltf/tiny_gltf.h>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include <map>
#include <mesh/mesh.hpp>
#include <model/animation-data.hpp>
#include <set>
#include <unordered_map>
#include <vector>

// Ensure proper namespace usage for BoneNode and BoneInfo
using our::BoneInfo;
using our::BoneNode;
using our::TranslationKeyframe;
using our::RotationKeyframe;
using our::ScaleKeyframe;
using our::InterpolationType;
using our::Channel;


// Helper function to print matrices for debugging
void printMatrix(const glm::mat4 &matrix, const std::string &name) {
    std::cout << name << ":\n";
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            std::cout << matrix[i][j] << " ";
        }
        std::cout << "\n";
    }
    std::cout << std::endl;
}

// Helper function to print bone hierarchy
void printBoneHierarchy(BoneNode *node, int depth = 0) {
    if (!node)
        return;

    std::string indent(depth * 2, ' ');
    std::cout << indent << "- " << node->name << " (index: " << node->index << ")" << std::endl;

    for (auto child : node->children) {
        printBoneHierarchy(child, depth + 1);
    }
}

// Helper function to verify transform components
void printNodeTransform(const tinygltf::Node &node, const std::string &prefix) {
    if (!node.translation.empty()) {
        std::cout << prefix << "Translation: " << node.translation[0] << ", " << node.translation[1] << ", "
                  << node.translation[2] << "\n";
    }

    if (!node.rotation.empty()) {
        std::cout << prefix << "Rotation (quaternion): " << node.rotation[0] << ", " << node.rotation[1] << ", "
                  << node.rotation[2] << ", " << node.rotation[3] << "\n";
    }

    if (!node.scale.empty()) {
        std::cout << prefix << "Scale: " << node.scale[0] << ", " << node.scale[1] << ", " << node.scale[2] << "\n";
    }

    if (!node.matrix.empty()) {
        std::cout << prefix << "Matrix: \n";
        for (int i = 0; i < 4; i++) {
            std::cout << prefix << "  ";
            for (int j = 0; j < 4; j++) {
                std::cout << node.matrix[i * 4 + j] << " ";
            }
            std::cout << "\n";
        }
    }
}

// Helper function to validate skin data
bool validateSkinData(const tinygltf::Model &model, const tinygltf::Skin &skin) {
    if (skin.joints.empty()) {
        std::cerr << "Error: No joints found in skin" << std::endl;
        return false;
    }

    if (skin.inverseBindMatrices != -1) {
        const auto &accessor = model.accessors[skin.inverseBindMatrices];
        if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
            std::cerr << "Error: Inverse bind matrices must be float type" << std::endl;
            return false;
        }
        if (accessor.type != TINYGLTF_TYPE_MAT4) {
            std::cerr << "Error: Inverse bind matrices must be mat4 type" << std::endl;
            return false;
        }
        if (accessor.count != skin.joints.size()) {
            std::cerr << "Error: Number of inverse bind matrices does not match number of joints" << std::endl;
            return false;
        }
    }

    return true;
}

// Helper function to build node hierarchy
void buildNodeHierarchy(const tinygltf::Model &model, const std::vector<int> &joints,
                        std::map<int, BoneNode *> &nodeToBone) {
    // First pass: Create all bone nodes
    for (int jointIndex : joints) {
        const auto &node = model.nodes[jointIndex];

        BoneNode *boneNode = new BoneNode();
        boneNode->name = node.name;
        boneNode->index = jointIndex;

        // Calculate local transform
        glm::mat4 localTransform(1.0f);

        if (!node.matrix.empty()) {
            localTransform = glm::make_mat4(node.matrix.data());
        } else {
            if (!node.translation.empty()) {
                localTransform = glm::translate(
                    localTransform, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
            }

            if (!node.rotation.empty()) {
                glm::quat rotation(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
                localTransform = localTransform * glm::mat4_cast(rotation);
            }

            if (!node.scale.empty()) {
                localTransform = glm::scale(localTransform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
            }
        }

        boneNode->localTransform = localTransform;
        nodeToBone[jointIndex] = boneNode;
    }

    // Second pass: Build hierarchy
    for (int jointIndex : joints) {
        const auto &node = model.nodes[jointIndex];
        BoneNode *boneNode = nodeToBone[jointIndex];

        for (int childIndex : node.children) {
            auto it = nodeToBone.find(childIndex);
            if (it != nodeToBone.end()) {
                boneNode->children.push_back(it->second);
            }
        }
    }
}

void loadKeyframeData(const tinygltf::Model &model, const tinygltf::AnimationChannel &channel,
                      const tinygltf::AnimationSampler &sampler, our::Channel &animChannel) {

    const auto &timeAccessor = model.accessors[sampler.input];
    const auto &valueAccessor = model.accessors[sampler.output];
    const auto &timeBufferView = model.bufferViews[timeAccessor.bufferView];
    const auto &valueBufferView = model.bufferViews[valueAccessor.bufferView];
    const auto &timeBuffer = model.buffers[timeBufferView.buffer];
    const auto &valueBuffer = model.buffers[valueBufferView.buffer];

    const float *times =
        reinterpret_cast<const float *>(&timeBuffer.data[timeBufferView.byteOffset + timeAccessor.byteOffset]);
    const float *values =
        reinterpret_cast<const float *>(&valueBuffer.data[valueBufferView.byteOffset + valueAccessor.byteOffset]);

    size_t timeStride = timeBufferView.byteStride ? timeBufferView.byteStride : sizeof(float);
    size_t valueStride = valueBufferView.byteStride
                             ? valueBufferView.byteStride
                             : (channel.target_path == "rotation" ? sizeof(float) * 4 : sizeof(float) * 3);

    for (size_t i = 0; i < timeAccessor.count; i++) {
        float time = times[i];

        if (channel.target_path == "translation") {
            TranslationKeyframe keyframe;
            keyframe.time = time;
            keyframe.value = glm::vec3(values[i * 3], values[i * 3 + 1], values[i * 3 + 2]);
            keyframe.interpolationType = animChannel.interpolationType;
            animChannel.translationKeys.push_back(keyframe);
        } else if (channel.target_path == "rotation") {
            RotationKeyframe keyframe;
            keyframe.time = time;
            keyframe.value = glm::quat(values[i * 4 + 3], // w
                                       values[i * 4],     // x
                                       values[i * 4 + 1], // y
                                       values[i * 4 + 2]  // z
            );
            keyframe.interpolationType = animChannel.interpolationType;
            animChannel.rotationKeys.push_back(keyframe);
        } else if (channel.target_path == "scale") {
            ScaleKeyframe keyframe;
            keyframe.time = time;
            keyframe.value = glm::vec3(values[i * 3], values[i * 3 + 1], values[i * 3 + 2]);
            keyframe.interpolationType = animChannel.interpolationType;
            animChannel.scaleKeys.push_back(keyframe);
        }
    }
}


// Helper function to load inverse bind matrices
void loadInverseBindMatrices(const tinygltf::Model &model, const tinygltf::Skin &skin,
                             std::map<int, BoneInfo> &boneInfoMap) {
    if (skin.inverseBindMatrices != -1) {
        const auto &accessor = model.accessors[skin.inverseBindMatrices];
        const auto &bufferView = model.bufferViews[accessor.bufferView];
        const auto &buffer = model.buffers[bufferView.buffer];

        size_t stride = bufferView.byteStride ? bufferView.byteStride : sizeof(float) * 16;

        for (size_t i = 0; i < skin.joints.size(); i++) {
            const float *matrixData =
                reinterpret_cast<const float *>(&buffer.data[bufferView.byteOffset + accessor.byteOffset + i * stride]);

            BoneInfo boneInfo;
            boneInfo.id = skin.joints[i];
            boneInfo.offsetMatrix = glm::make_mat4(matrixData);
            boneInfoMap[skin.joints[i]] = boneInfo;
        }
    } else {
        // If no inverse bind matrices are provided, use identity matrices
        for (size_t i = 0; i < skin.joints.size(); i++) {
            BoneInfo boneInfo;
            boneInfo.id = skin.joints[i];
            boneInfo.offsetMatrix = glm::mat4(1.0f);
            boneInfoMap[skin.joints[i]] = boneInfo;
        }
    }
}

our::Mesh *our::mesh_utils::loadOBJ(const std::string &filename) {

    // The data that we will use to initialize our mesh
    std::vector<our::Vertex> vertices;
    std::vector<GLuint> elements;

    // Since the OBJ can have duplicated vertices, we make them unique using this map
    // The key is the vertex, the value is its index in the vector "vertices".
    // That index will be used to populate the "elements" vector.
    std::unordered_map<our::Vertex, GLuint> vertex_map;

    // The data loaded by Tiny OBJ Loader
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str())) {
        std::cerr << "Failed to load obj file \"" << filename << "\" due to error: " << err << std::endl;
        return nullptr;
    }
    if (!warn.empty()) {
        std::cout << "WARN while loading obj file \"" << filename << "\": " << warn << std::endl;
    }

    // An obj file can have multiple shapes where each shape can have its own material
    // Ideally, we would load each shape into a separate mesh or store the start and end of it in the element buffer to
    // be able to draw each shape separately But we ignored this fact since we don't plan to use multiple materials in
    // the examples
    for (const auto &shape : shapes) {
        for (const auto &index : shape.mesh.indices) {
            Vertex vertex = {};

            // Read the data for a vertex from the "attrib" object
            vertex.position = {attrib.vertices[3 * index.vertex_index + 0], attrib.vertices[3 * index.vertex_index + 1],
                               attrib.vertices[3 * index.vertex_index + 2]};

            vertex.normal = {attrib.normals[3 * index.normal_index + 0], attrib.normals[3 * index.normal_index + 1],
                             attrib.normals[3 * index.normal_index + 2]};

            vertex.tex_coord = {attrib.texcoords[2 * index.texcoord_index + 0],
                                attrib.texcoords[2 * index.texcoord_index + 1]};

            vertex.color = {attrib.colors[3 * index.vertex_index + 0] * 255,
                            attrib.colors[3 * index.vertex_index + 1] * 255,
                            attrib.colors[3 * index.vertex_index + 2] * 255, 255};

            // See if we already stored a similar vertex
            auto it = vertex_map.find(vertex);
            if (it == vertex_map.end()) {
                // if no, add it to the vertices and record its index
                auto new_vertex_index = static_cast<GLuint>(vertices.size());
                vertex_map[vertex] = new_vertex_index;
                elements.push_back(new_vertex_index);
                vertices.push_back(vertex);
            } else {
                // if yes, just add its index in the elements vector
                elements.push_back(it->second);
            }
        }
    }

    return new our::Mesh(vertices, elements);
}

our::Mesh *our::mesh_utils::loadGLTF(const std::string &filename) {
    // The data that we will use to initialize our mesh
    std::vector<our::Vertex> vertices;
    std::vector<GLuint> elements;

    // Since we may have duplicated vertices, we can make them unique
    std::unordered_map<our::Vertex, GLuint> vertex_map;

    // Load GLTF model
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);

    if (!warn.empty()) {
        std::cout << "WARN while loading gltf file \"" << filename << "\": " << warn << std::endl;
    }

    if (!err.empty()) {
        std::cerr << "ERROR loading gltf file \"" << filename << "\": " << err << std::endl;
        return nullptr;
    }

    if (!ret) {
        std::cerr << "Failed to load gltf file \"" << filename << "\"" << std::endl;
        return nullptr;
    }

    std::cout << "Loading GLTF file \"" << filename << "\"" << std::endl;

    // Process all meshes in the GLTF file
    for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex) {
        const tinygltf::Mesh &mesh = model.meshes[meshIndex];

        // Process each primitive in the mesh
        for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); ++primitiveIndex) {
            const tinygltf::Primitive &primitive = mesh.primitives[primitiveIndex];

            // Get accessor indices for attributes
            int positionAccessorIndex = primitive.attributes.find("POSITION") != primitive.attributes.end()
                                            ? primitive.attributes.at("POSITION")
                                            : -1;
            int normalAccessorIndex = primitive.attributes.find("NORMAL") != primitive.attributes.end()
                                          ? primitive.attributes.at("NORMAL")
                                          : -1;
            int texcoordAccessorIndex = primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()
                                            ? primitive.attributes.at("TEXCOORD_0")
                                            : -1;
            int colorAccessorIndex = primitive.attributes.find("COLOR_0") != primitive.attributes.end()
                                         ? primitive.attributes.at("COLOR_0")
                                         : -1;
            int boneIndexAccessorIndex = primitive.attributes.find("JOINTS_0") != primitive.attributes.end()
                                             ? primitive.attributes.at("JOINTS_0")
                                             : -1;
            int boneWeightAccessorIndex = primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end()
                                              ? primitive.attributes.at("WEIGHTS_0")
                                              : -1;

            // Validate bone data
            bool hasBoneIndices = boneIndexAccessorIndex != -1;
            bool hasBoneWeights = boneWeightAccessorIndex != -1;

            if (hasBoneIndices != hasBoneWeights) {
                std::cerr << "Error: Mesh has mismatched bone data" << std::endl;
                return nullptr;
            }

            // Get accessors and buffer views for all attributes
            const tinygltf::Accessor *positionAccessor =
                positionAccessorIndex != -1 ? &model.accessors[positionAccessorIndex] : nullptr;
            const tinygltf::BufferView *positionBufferView =
                positionAccessor ? &model.bufferViews[positionAccessor->bufferView] : nullptr;
            const tinygltf::Buffer *positionBuffer =
                positionBufferView ? &model.buffers[positionBufferView->buffer] : nullptr;

            const tinygltf::Accessor *normalAccessor =
                normalAccessorIndex != -1 ? &model.accessors[normalAccessorIndex] : nullptr;
            const tinygltf::BufferView *normalBufferView =
                normalAccessor ? &model.bufferViews[normalAccessor->bufferView] : nullptr;
            const tinygltf::Buffer *normalBuffer =
                normalBufferView ? &model.buffers[normalBufferView->buffer] : nullptr;

            const tinygltf::Accessor *texcoordAccessor =
                texcoordAccessorIndex != -1 ? &model.accessors[texcoordAccessorIndex] : nullptr;
            const tinygltf::BufferView *texcoordBufferView =
                texcoordAccessor ? &model.bufferViews[texcoordAccessor->bufferView] : nullptr;
            const tinygltf::Buffer *texcoordBuffer =
                texcoordBufferView ? &model.buffers[texcoordBufferView->buffer] : nullptr;

            const tinygltf::Accessor *boneIndexAccessor =
                boneIndexAccessorIndex != -1 ? &model.accessors[boneIndexAccessorIndex] : nullptr;
            const tinygltf::BufferView *boneIndexBufferView =
                boneIndexAccessor ? &model.bufferViews[boneIndexAccessor->bufferView] : nullptr;
            const tinygltf::Buffer *boneIndexBuffer =
                boneIndexBufferView ? &model.buffers[boneIndexBufferView->buffer] : nullptr;

            const tinygltf::Accessor *boneWeightAccessor =
                boneWeightAccessorIndex != -1 ? &model.accessors[boneWeightAccessorIndex] : nullptr;
            const tinygltf::BufferView *boneWeightBufferView =
                boneWeightAccessor ? &model.bufferViews[boneWeightAccessor->bufferView] : nullptr;
            const tinygltf::Buffer *boneWeightBuffer =
                boneWeightBufferView ? &model.buffers[boneWeightBufferView->buffer] : nullptr;

            // Calculate strides
            size_t positionStride = positionBufferView->byteStride ? positionBufferView->byteStride : sizeof(float) * 3;
            size_t normalStride =
                normalBufferView ? (normalBufferView->byteStride ? normalBufferView->byteStride : sizeof(float) * 3)
                                 : 0;
            size_t texcoordStride =
                texcoordBufferView
                    ? (texcoordBufferView->byteStride ? texcoordBufferView->byteStride : sizeof(float) * 2)
                    : 0;
            size_t boneIndexStride =
                boneIndexBufferView
                    ? (boneIndexBufferView->byteStride ? boneIndexBufferView->byteStride : sizeof(uint16_t) * 4)
                    : 0;
            size_t boneWeightStride =
                boneWeightBufferView
                    ? (boneWeightBufferView->byteStride ? boneWeightBufferView->byteStride : sizeof(float) * 4)
                    : 0;

            // Process vertices
            size_t vertexCount = positionAccessor->count;
            for (size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
                our::Vertex vertex;

                // Position (required)
                const float *positions = reinterpret_cast<const float *>(
                    &positionBuffer->data[positionBufferView->byteOffset + positionAccessor->byteOffset +
                                          vertexIndex * positionStride]);
                vertex.position = glm::vec3(positions[0], positions[1], positions[2]);

                // Normal (optional)
                if (normalAccessor) {
                    const float *normals = reinterpret_cast<const float *>(
                        &normalBuffer->data[normalBufferView->byteOffset + normalAccessor->byteOffset +
                                            vertexIndex * normalStride]);
                    vertex.normal = glm::vec3(normals[0], normals[1], normals[2]);
                } else {
                    vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);
                }

                // Texture coordinates (optional)
                if (texcoordAccessor) {
                    const float *texcoords = reinterpret_cast<const float *>(
                        &texcoordBuffer->data[texcoordBufferView->byteOffset + texcoordAccessor->byteOffset +
                                              vertexIndex * texcoordStride]);
                    vertex.tex_coord = glm::vec2(texcoords[0], 1.0f - texcoords[1]);
                } else {
                    vertex.tex_coord = glm::vec2(0.0f);
                }

                // Bone indices and weights (optional)
                if (hasBoneIndices && hasBoneWeights) {
                    const uint16_t *boneIndices = reinterpret_cast<const uint16_t *>(
                        &boneIndexBuffer->data[boneIndexBufferView->byteOffset + boneIndexAccessor->byteOffset +
                                               vertexIndex * boneIndexStride]);
                    const float *boneWeights = reinterpret_cast<const float *>(
                        &boneWeightBuffer->data[boneWeightBufferView->byteOffset + boneWeightAccessor->byteOffset +
                                                vertexIndex * boneWeightStride]);

                    vertex.boneIndices = glm::ivec4(boneIndices[0], boneIndices[1], boneIndices[2], boneIndices[3]);
                    vertex.boneWeights = glm::vec4(boneWeights[0], boneWeights[1], boneWeights[2], boneWeights[3]);

                    // Debug output for first few vertices
                    if (vertexIndex < 5) {
                        std::cout << "Vertex " << vertexIndex << " bone data:" << std::endl;
                        std::cout << "  Indices: " << vertex.boneIndices.x << ", " << vertex.boneIndices.y << ", "
                                  << vertex.boneIndices.z << ", " << vertex.boneIndices.w << std::endl;
                        std::cout << "  Weights: " << vertex.boneWeights.x << ", " << vertex.boneWeights.y << ", "
                                  << vertex.boneWeights.z << ", " << vertex.boneWeights.w << std::endl;
                    }
                } else {
                    vertex.boneIndices = glm::ivec4(0);
                    vertex.boneWeights = glm::vec4(0.0f);
                }

                // Add vertex to the mesh
                auto it = vertex_map.find(vertex);
                if (it == vertex_map.end()) {
                    GLuint index = static_cast<GLuint>(vertices.size());
                    vertex_map[vertex] = index;
                    vertices.push_back(vertex);
                }
            }

            // Process indices
            if (primitive.indices >= 0) {
                const tinygltf::Accessor &indexAccessor = model.accessors[primitive.indices];
                const tinygltf::BufferView &indexBufferView = model.bufferViews[indexAccessor.bufferView];
                const tinygltf::Buffer &indexBuffer = model.buffers[indexBufferView.buffer];

                size_t indexCount = indexAccessor.count;
                for (size_t i = 0; i < indexCount; ++i) {
                    uint32_t index = 0;
                    switch (indexAccessor.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                        const uint16_t *indices = reinterpret_cast<const uint16_t *>(
                            &indexBuffer
                                 .data[indexBufferView.byteOffset + indexAccessor.byteOffset + i * sizeof(uint16_t)]);
                        index = *indices;
                        break;
                    }
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                        const uint32_t *indices = reinterpret_cast<const uint32_t *>(
                            &indexBuffer
                                 .data[indexBufferView.byteOffset + indexAccessor.byteOffset + i * sizeof(uint32_t)]);
                        index = *indices;
                        break;
                    }
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                        const uint8_t *indices = reinterpret_cast<const uint8_t *>(
                            &indexBuffer
                                 .data[indexBufferView.byteOffset + indexAccessor.byteOffset + i * sizeof(uint8_t)]);
                        index = *indices;
                        break;
                    }
                    default:
                        std::cerr << "Unsupported index component type" << std::endl;
                        return nullptr;
                    }

                    const our::Vertex &vertex = vertices[index];
                    auto it = vertex_map.find(vertex);
                    if (it != vertex_map.end()) {
                        elements.push_back(it->second);
                    }
                }
            }
        }
    }

    if (vertices.empty() || elements.empty()) {
        std::cerr << "No valid mesh data found in GLTF file" << std::endl;
        return nullptr;
    }

    return new our::Mesh(vertices, elements);
}

our::AnimationData *our::mesh_utils::loadGLTFAnimation(const std::string &filename) {
    AnimationData *animationData = new AnimationData();
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);

    if (!warn.empty()) {
        std::cout << "WARN while loading gltf file: " << warn << std::endl;
    }

    if (!err.empty() || !ret) {
        std::cerr << "Failed to load gltf file: " << err << std::endl;
        return nullptr;
    }

    // Verify model has skins
    if (model.skins.empty()) {
        std::cerr << "No skins found in the model" << std::endl;
        return nullptr;
    }

    const auto &skin = model.skins[0];
    std::cout << "\n=== Skin Data Verification ===" << std::endl;
    std::cout << "Number of joints: " << skin.joints.size() << std::endl;
    std::cout << "Inverse bind matrices accessor index: " << skin.inverseBindMatrices << std::endl;

    // Validate skin data
    if (!validateSkinData(model, skin)) {
        return nullptr;
    }

    // Create bone hierarchy
    std::map<int, BoneNode *> nodeToBone;
    buildNodeHierarchy(model, skin.joints, nodeToBone);

    // Load inverse bind matrices
    loadInverseBindMatrices(model, skin, animationData->boneInfoMap);

    // Find root bone
    std::set<int> childIndices;
    for (const auto &joint : skin.joints) {
        const auto &node = model.nodes[joint];
        for (int child : node.children) {
            childIndices.insert(child);
        }
    }

    for (const auto &joint : skin.joints) {
        if (childIndices.find(joint) == childIndices.end()) {
            animationData->rootBone = nodeToBone[joint];
            break;
        }
    }

    // Print verification info
    std::cout << "\n=== Bone Hierarchy Verification ===" << std::endl;
    printBoneHierarchy(animationData->rootBone);

    // Load animations
    for (const auto &animation : model.animations) {
        AnimationClip clip;
        clip.name = animation.name;
        clip.duration = 0.0f;
        clip.speed = 1.0f;
        clip.isLooping = true;

        for (const auto &channel : animation.channels) {
            Channel animChannel;
            animChannel.nodeIndex = channel.target_node;
            animChannel.name = model.nodes[channel.target_node].name;

            const auto &sampler = animation.samplers[channel.sampler];

            // Set interpolation type
            if (sampler.interpolation == "LINEAR")
                animChannel.interpolationType = InterpolationType::LINEAR;
            else if (sampler.interpolation == "STEP")
                animChannel.interpolationType = InterpolationType::STEP;
            else if (sampler.interpolation == "CUBICSPLINE")
                animChannel.interpolationType = InterpolationType::CUBICSPLINE;

            // Set path type
            if (channel.target_path == "translation")
                animChannel.path = Channel::PathType::Translation;
            else if (channel.target_path == "rotation")
                animChannel.path = Channel::PathType::Rotation;
            else if (channel.target_path == "scale")
                animChannel.path = Channel::PathType::Scale;
            else
                continue;

            // Load keyframe data
            loadKeyframeData(model, channel, sampler, animChannel);

            // Update timing info
            if (!animChannel.translationKeys.empty()) {
                animChannel.startTime = animChannel.translationKeys.front().time;
                animChannel.endTime = animChannel.translationKeys.back().time;
            } else if (!animChannel.rotationKeys.empty()) {
                animChannel.startTime = animChannel.rotationKeys.front().time;
                animChannel.endTime = animChannel.rotationKeys.back().time;
            } else if (!animChannel.scaleKeys.empty()) {
                animChannel.startTime = animChannel.scaleKeys.front().time;
                animChannel.endTime = animChannel.scaleKeys.back().time;
            }

            clip.channels.push_back(animChannel);
        }
    }

    // Print animation verification info
    std::cout << "\n=== Animation Data Verification ===" << std::endl;
    for (const auto &[name, clip] : animationData->animationClips) {
        std::cout << "Animation: " << name << std::endl;
        std::cout << "Duration: " << clip.duration << std::endl;
        std::cout << "Channels: " << clip.channels.size() << std::endl;
    }

    return animationData;
}

// Create a sphere (the vertex order in the triangles are CCW from the outside)
// Segments define the number of divisions on the both the latitude and the longitude
our::Mesh *our::mesh_utils::sphere(const glm::ivec2 &segments) {
    std::vector<our::Vertex> vertices;
    std::vector<GLuint> elements;

    // We populate the sphere vertices by looping over its longitude and latitude
    for (int lat = 0; lat <= segments.y; lat++) {
        float v = (float)lat / segments.y;
        float pitch = v * glm::pi<float>() - glm::half_pi<float>();
        float cos = glm::cos(pitch), sin = glm::sin(pitch);
        for (int lng = 0; lng <= segments.x; lng++) {
            float u = (float)lng / segments.x;
            float yaw = u * glm::two_pi<float>();
            glm::vec3 normal = {cos * glm::cos(yaw), sin, cos * glm::sin(yaw)};
            glm::vec3 position = normal;
            glm::vec2 tex_coords = glm::vec2(u, v);
            our::Color color = our::Color(255, 255, 255, 255);
            vertices.push_back({position, color, tex_coords, normal});
        }
    }

    for (int lat = 1; lat <= segments.y; lat++) {
        int start = lat * (segments.x + 1);
        for (int lng = 1; lng <= segments.x; lng++) {
            int prev_lng = lng - 1;
            elements.push_back(lng + start);
            elements.push_back(lng + start - segments.x - 1);
            elements.push_back(prev_lng + start - segments.x - 1);
            elements.push_back(prev_lng + start - segments.x - 1);
            elements.push_back(prev_lng + start);
            elements.push_back(lng + start);
        }
    }

    return new our::Mesh(vertices, elements);
}

unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
unsigned int sphereVAO = 0;
unsigned int indexCount;
unsigned int quadVAO = 0;
unsigned int quadVBO = 0;

// renderCube() renders a 1x1 3D cube in NDC.
// -------------------------------------------------

void our::mesh_utils::renderCube() {
    // initialize (if necessary)
    if (cubeVAO == 0) {
        float vertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   // top-right
            1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,  // bottom-right
            1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   // top-right
            -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,  // top-left
            // front face
            -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left
            1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,  // bottom-right
            1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   // top-right
            1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   // top-right
            -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,  // top-left
            -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left
            // left face
            -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-right
            -1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,  // top-left
            -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  // bottom-right
            -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-right
                                                                // right face
            1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,     // top-left
            1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,   // bottom-right
            1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,    // top-right
            1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,   // bottom-right
            1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,     // top-left
            1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,    // bottom-left
            // bottom face
            -1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, // top-right
            1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,  // top-left
            1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   // bottom-left
            1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   // bottom-left
            -1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,  // bottom-right
            -1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, // top-right
            // top face
            -1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // top-left
            1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom-right
            1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,  // top-right
            1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom-right
            -1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // top-left
            -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f   // bottom-left
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);

        glEnableVertexAttribArray(ATTRIB_LOC_POSITION);
        glVertexAttribPointer(ATTRIB_LOC_POSITION, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);

        glEnableVertexAttribArray(ATTRIB_LOC_NORMAL);
        glVertexAttribPointer(ATTRIB_LOC_NORMAL, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));

        glEnableVertexAttribArray(ATTRIB_LOC_TEXCOORD);
        glVertexAttribPointer(ATTRIB_LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                              (void *)(6 * sizeof(float)));

        glEnableVertexAttribArray(ATTRIB_LOC_BONEWEIGHTS);
        glVertexAttribPointer(ATTRIB_LOC_BONEWEIGHTS, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                              (void *)(7 * sizeof(float)));

        glEnableVertexAttribArray(ATTRIB_LOC_BONEINDICES);
        glVertexAttribIPointer(ATTRIB_LOC_BONEINDICES, 4, GL_INT, 8 * sizeof(float), (void *)(6 * sizeof(float)));

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
void our::mesh_utils::renderQuad() {
    if (quadVAO == 0) {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
            1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(ATTRIB_LOC_POSITION);
        glVertexAttribPointer(ATTRIB_LOC_POSITION, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(ATTRIB_LOC_TEXCOORD);
        glVertexAttribPointer(ATTRIB_LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                              (void *)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

our::Mesh *our::mesh_utils::createJointVisualizationMesh(float radius, int segments) {
    std::vector<our::Vertex> vertices;
    std::vector<GLuint> elements;

    // Create sphere vertices
    for (int lat = 0; lat <= segments; lat++) {
        float theta = lat * glm::pi<float>() / segments;
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);

        for (int lon = 0; lon <= segments; lon++) {
            float phi = lon * 2 * glm::pi<float>() / segments;
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);

            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;

            our::Vertex vertex;
            vertex.position = glm::vec3(x, y, z) * radius;
            vertex.normal = glm::vec3(x, y, z);
            vertex.color = {255, 0, 0, 255}; // Red color
            vertex.tex_coord = glm::vec2((float)lon / segments, (float)lat / segments);

            vertices.push_back(vertex);
        }
    }

    // Create indices
    for (int lat = 0; lat < segments; lat++) {
        for (int lon = 0; lon < segments; lon++) {
            int current = lat * (segments + 1) + lon;
            int next = current + segments + 1;

            elements.push_back(current);
            elements.push_back(next);
            elements.push_back(current + 1);

            elements.push_back(next);
            elements.push_back(next + 1);
            elements.push_back(current + 1);
        }
    }

    return new our::Mesh(vertices, elements);
}

our::Mesh *our::mesh_utils::createBoneVisualizationMesh() {
    std::vector<our::Vertex> vertices;
    std::vector<GLuint> elements;

    const int segments = 8;
    const float baseRadius = 0.025f;
    const float tipRadius = 0.0f;
    const float height = 1.0f;

    // Create vertices for the cone
    // Base vertices
    for (int i = 0; i <= segments; i++) {
        float angle = (float)i / segments * 2.0f * glm::pi<float>();
        float x = cos(angle) * baseRadius;
        float z = sin(angle) * baseRadius;

        our::Vertex vertex;
        vertex.position = glm::vec3(x, 0.0f, z);
        vertex.normal = glm::normalize(glm::vec3(x, 0.0f, z));
        vertex.color = {0, 255, 255, 255}; // Cyan color
        vertex.tex_coord = glm::vec2((float)i / segments, 0.0f);

        vertices.push_back(vertex);
    }

    // Tip vertices
    for (int i = 0; i <= segments; i++) {
        float angle = (float)i / segments * 2.0f * glm::pi<float>();
        float x = cos(angle) * tipRadius;
        float z = sin(angle) * tipRadius;

        our::Vertex vertex;
        vertex.position = glm::vec3(x, height, z);
        vertex.normal = glm::normalize(glm::vec3(x, height, z));
        vertex.color = {0, 255, 255, 255}; // Cyan color
        vertex.tex_coord = glm::vec2((float)i / segments, 1.0f);

        vertices.push_back(vertex);
    }

    // Create indices
    for (int i = 0; i < segments; i++) {
        // Bottom face triangles
        elements.push_back(i);
        elements.push_back(i + 1);
        elements.push_back(i + segments + 1);

        elements.push_back(i + segments + 1);
        elements.push_back(i + 1);
        elements.push_back(i + segments + 2);
    }

    return new our::Mesh(vertices, elements);
}

// Add the bone transform calculation helper
glm::mat4 our::mesh_utils::calculateBoneTransform(const glm::vec3 &start, const glm::vec3 &end) {
    glm::vec3 direction = end - start;
    float length = glm::length(direction);
    direction = glm::normalize(direction);

    // Create rotation matrix that aligns the bone with the direction
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(direction, up));
    up = glm::normalize(glm::cross(right, direction));

    glm::mat4 rotation(glm::vec4(right, 0.0f), glm::vec4(direction, 0.0f), glm::vec4(up, 0.0f),
                       glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

    // Create scale matrix for bone length
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, length, 1.0f));

    // Create translation matrix to position bone
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), start);

    return translation * rotation * scale;
}

