#include "mesh-utils.hpp"

// We will use "Tiny OBJ Loader" to read and process '.obj" files
#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobj/tiny_obj_loader.h>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tinygltf/tiny_gltf.h>

#include <iostream>
#include <mesh/mesh.hpp>
#include <model/animation-data.hpp>
#include <unordered_map>
#include <vector>

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

    // Process all meshes in the GLTF file (assuming we want to combine all meshes)
    for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex) {
        const tinygltf::Mesh &mesh = model.meshes[meshIndex];

        // Process each primitive in the mesh
        for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); ++primitiveIndex) {
            const tinygltf::Primitive &primitive = mesh.primitives[primitiveIndex];

            // Get the accessor indices for the attributes we need
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

            // Get the indices accessor
            int indicesAccessorIndex = primitive.indices;

            // Skip primitives with no positions
            if (positionAccessorIndex == -1)
                continue;

            // Get accessors
            const tinygltf::Accessor &positionAccessor = model.accessors[positionAccessorIndex];
            const tinygltf::BufferView &positionBufferView = model.bufferViews[positionAccessor.bufferView];
            const tinygltf::Buffer &positionBuffer = model.buffers[positionBufferView.buffer];

            // Calculate position stride - use the bufferView stride if specified, otherwise calculate it
            int positionStride = positionBufferView.byteStride
                                     ? positionBufferView.byteStride
                                     : (positionAccessor.type == TINYGLTF_TYPE_VEC3 ? 3 * sizeof(float) : 0);

            // Get view for normal if it exists
            const tinygltf::Accessor *normalAccessor = nullptr;
            const tinygltf::BufferView *normalBufferView = nullptr;
            const tinygltf::Buffer *normalBuffer = nullptr;
            int normalStride = 0;
            if (normalAccessorIndex != -1) {
                normalAccessor = &model.accessors[normalAccessorIndex];
                normalBufferView = &model.bufferViews[normalAccessor->bufferView];
                normalBuffer = &model.buffers[normalBufferView->buffer];
                normalStride = normalBufferView->byteStride
                                   ? normalBufferView->byteStride
                                   : (normalAccessor->type == TINYGLTF_TYPE_VEC3 ? 3 * sizeof(float) : 0);
            }

            // Get view for texcoord if it exists
            const tinygltf::Accessor *texcoordAccessor = nullptr;
            const tinygltf::BufferView *texcoordBufferView = nullptr;
            const tinygltf::Buffer *texcoordBuffer = nullptr;
            int texcoordStride = 0;
            if (texcoordAccessorIndex != -1) {
                texcoordAccessor = &model.accessors[texcoordAccessorIndex];
                texcoordBufferView = &model.bufferViews[texcoordAccessor->bufferView];
                texcoordBuffer = &model.buffers[texcoordBufferView->buffer];
                texcoordStride = texcoordBufferView->byteStride
                                     ? texcoordBufferView->byteStride
                                     : (texcoordAccessor->type == TINYGLTF_TYPE_VEC2 ? 2 * sizeof(float) : 0);
            }

            // Get view for color if it exists
            const tinygltf::Accessor *colorAccessor = nullptr;
            const tinygltf::BufferView *colorBufferView = nullptr;
            const tinygltf::Buffer *colorBuffer = nullptr;
            int colorStride = 0;
            if (colorAccessorIndex != -1) {
                colorAccessor = &model.accessors[colorAccessorIndex];
                colorBufferView = &model.bufferViews[colorAccessor->bufferView];
                colorBuffer = &model.buffers[colorBufferView->buffer];
                colorStride = colorBufferView->byteStride
                                  ? colorBufferView->byteStride
                                  : (colorAccessor->type == TINYGLTF_TYPE_VEC3
                                         ? 3 * sizeof(float)
                                         : (colorAccessor->type == TINYGLTF_TYPE_VEC4 ? 4 * sizeof(float) : 0));
            }

            // Get indices
            const tinygltf::Accessor &indicesAccessor = model.accessors[indicesAccessorIndex];
            const tinygltf::BufferView &indicesBufferView = model.bufferViews[indicesAccessor.bufferView];
            const tinygltf::Buffer &indicesBuffer = model.buffers[indicesBufferView.buffer];

            // Calculate vertex count
            size_t vertexCount = positionAccessor.count;

            // Create vertices
            std::vector<our::Vertex> primitiveVertices;
            primitiveVertices.reserve(vertexCount);

            for (size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
                our::Vertex vertex = {};

                // Position (required)
                const float *positions = reinterpret_cast<const float *>(
                    &positionBuffer
                         .data[positionBufferView.byteOffset + positionAccessor.byteOffset +
                               (positionStride ? vertexIndex * positionStride : vertexIndex * 3 * sizeof(float))]);
                vertex.position = {positions[0], positions[1], positions[2]};

                // Normal (optional)
                if (normalAccessor) {
                    const float *normals = reinterpret_cast<const float *>(
                        &normalBuffer
                             ->data[normalBufferView->byteOffset + normalAccessor->byteOffset +
                                    (normalStride ? vertexIndex * normalStride : vertexIndex * 3 * sizeof(float))]);
                    vertex.normal = {normals[0], normals[1], normals[2]};
                } else {
                    vertex.normal = {0.0f, 0.0f, 1.0f}; // Default normal
                }

                // Texture coordinates (optional)
                if (texcoordAccessor) {
                    const float *texcoords = reinterpret_cast<const float *>(
                        &texcoordBuffer
                             ->data[texcoordBufferView->byteOffset + texcoordAccessor->byteOffset +
                                    (texcoordStride ? vertexIndex * texcoordStride : vertexIndex * 2 * sizeof(float))]);

                    // OpenGL expects texture coordinates with origin at bottom-left
                    // GLTF has origin at top-left, so we flip Y coordinate
                    vertex.tex_coord = {texcoords[0], 1.0f - texcoords[1]};
                } else {
                    vertex.tex_coord = {0.0f, 0.0f}; // Default UV
                }

                // Color (optional)
                if (colorAccessor) {
                    if (colorAccessor->type == TINYGLTF_TYPE_VEC3) {
                        // RGB color
                        const float *colors = reinterpret_cast<const float *>(
                            &colorBuffer
                                 ->data[colorBufferView->byteOffset + colorAccessor->byteOffset +
                                        (colorStride ? vertexIndex * colorStride : vertexIndex * 3 * sizeof(float))]);
                        vertex.color = {static_cast<uint8_t>(colors[0] * 255), static_cast<uint8_t>(colors[1] * 255),
                                        static_cast<uint8_t>(colors[2] * 255), 255};
                    } else if (colorAccessor->type == TINYGLTF_TYPE_VEC4) {
                        // RGBA color
                        const float *colors = reinterpret_cast<const float *>(
                            &colorBuffer
                                 ->data[colorBufferView->byteOffset + colorAccessor->byteOffset +
                                        (colorStride ? vertexIndex * colorStride : vertexIndex * 4 * sizeof(float))]);
                        vertex.color = {static_cast<uint8_t>(colors[0] * 255), static_cast<uint8_t>(colors[1] * 255),
                                        static_cast<uint8_t>(colors[2] * 255), static_cast<uint8_t>(colors[3] * 255)};
                    }
                } else {
                    vertex.color = {255, 255, 255, 255}; // Default color
                }

                // Add vertex to the primitive vertices
                primitiveVertices.push_back(vertex);
            }

            // Process indices and add to our elements list
            size_t indexCount = indicesAccessor.count;
            size_t baseVertex = vertices.size(); // Offset for the current primitive

            for (size_t indexIndex = 0; indexIndex < indexCount; ++indexIndex) {
                uint32_t vertexIndex = 0;

                if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    vertexIndex = *reinterpret_cast<const uint16_t *>(
                        &indicesBuffer.data[indicesBufferView.byteOffset + indicesAccessor.byteOffset +
                                            indexIndex * sizeof(uint16_t)]);
                } else if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    vertexIndex = *reinterpret_cast<const uint32_t *>(
                        &indicesBuffer.data[indicesBufferView.byteOffset + indicesAccessor.byteOffset +
                                            indexIndex * sizeof(uint32_t)]);
                } else if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    vertexIndex = *reinterpret_cast<const uint8_t *>(
                        &indicesBuffer.data[indicesBufferView.byteOffset + indicesAccessor.byteOffset +
                                            indexIndex * sizeof(uint8_t)]);
                } else {
                    continue; // Unsupported component type
                }

                // Get the Vertex from our primitive vertices
                const our::Vertex &primitiveVertex = primitiveVertices[vertexIndex];

                // Check if we've already added this vertex
                auto it = vertex_map.find(primitiveVertex);
                if (it == vertex_map.end()) {
                    // If not, add it to our global vertex list
                    GLuint newIndex = static_cast<GLuint>(vertices.size());
                    vertex_map[primitiveVertex] = newIndex;
                    vertices.push_back(primitiveVertex);
                    elements.push_back(newIndex);
                } else {
                    // If yes, just use its index
                    elements.push_back(it->second);
                }
            }
        }
    }

    // Create and return the mesh
    if (vertices.empty() || elements.empty()) {
        std::cerr << "No valid mesh data found in GLTF file \"" << filename << "\"" << std::endl;
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

    // Iterate through animations in the GLTF file
    for (const auto &animation : model.animations) {
        AnimationClip clip;
        clip.name = animation.name;

        for (const auto &channel : animation.channels) {
            Channel animChannel;
            animChannel.nodeIndex = channel.target_node;

            if (channel.target_path == "translation") {
                animChannel.path = Channel::PathType::Translation;
            } else if (channel.target_path == "rotation") {
                animChannel.path = Channel::PathType::Rotation;
            } else if (channel.target_path == "scale") {
                animChannel.path = Channel::PathType::Scale;
            } else {
                continue; // Unsupported path type
            }

            animChannel.interpolationType = animation.samplers[channel.sampler].interpolation;

            const auto &inputAccessor = model.accessors[animation.samplers[channel.sampler].input];
            const auto &outputAccessor = model.accessors[animation.samplers[channel.sampler].output];

            const auto &inputBufferView = model.bufferViews[inputAccessor.bufferView];
            const auto &outputBufferView = model.bufferViews[outputAccessor.bufferView];

            const auto &inputBuffer = model.buffers[inputBufferView.buffer];
            const auto &outputBuffer = model.buffers[outputBufferView.buffer];

            const float *inputData = reinterpret_cast<const float *>(
                &inputBuffer.data[inputBufferView.byteOffset + inputAccessor.byteOffset]);
            const float *outputData = reinterpret_cast<const float *>(
                &outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset]);

            for (size_t i = 0; i < inputAccessor.count; ++i) {
                animChannel.keyframeTimes.push_back(inputData[i]);

                if (animChannel.path == Channel::PathType::Translation ||
                    animChannel.path == Channel::PathType::Scale) {
                    glm::vec3 value(outputData[i * 3], outputData[i * 3 + 1], outputData[i * 3 + 2]);
                    animChannel.keyframeValues.push_back(value);
                } else if (animChannel.path == Channel::PathType::Rotation) {
                    glm::quat value(outputData[i * 4 + 3], outputData[i * 4], outputData[i * 4 + 1],
                                    outputData[i * 4 + 2]);
                    animChannel.keyframeValues.push_back(value);
                }
            }

            clip.channels.push_back(animChannel);
        }

        for (const auto &channel : clip.channels) {
            for (const auto &time : channel.keyframeTimes) {
                if (time > clip.duration) {
                    clip.duration = time;
                }
            }
        }

        animationData->animationClips[clip.name] = clip;
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