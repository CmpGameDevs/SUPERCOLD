#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#define MAX_BONE_INFLUENCE 4

namespace our {

// Since we may want to store colors in bytes instead of floats for efficiency,
// we are creating our own 32-bit R8G8B8A8 Color data type with the default GLM precision
typedef glm::vec<4, glm::uint8, glm::defaultp> Color;

struct Vertex {

    // --- Standard Attributes ---
    glm::vec3 position;  // Vertex position (x, y, z)
    Color color;         // The vertex color
    glm::vec2 tex_coord; //  Texture coordinates (u, v) - for mapping textures
    glm::vec3 normal;    // Vertex normal vector (nx, ny, nz) - for lighting calculations

    // --- Skeletal Animation Attributes
    // Bone IDs: Stores the indices of the bones that influence this vertex.
    int bone_ids[MAX_BONE_INFLUENCE];
    // Weights: Stores the influence of each corresponding bone in bond_ids.
    float weights[MAX_BONE_INFLUENCE];

    Vertex()
        : position(0.0f, 0.0f, 0.0f), normal(0.0f, 0.0f, 0.0f), tex_coord(0.0f, 0.0f), color(0.0f, 0.0f, 0.0f, 0.0f) {

        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
            bone_ids[i] = -1;
            weights[i] = 0.0f;
        }
    }

    /**
     * @brief Parameterized constructor for basic attributes.
     * Initializes skeletal animation attributes to default (no influence).
     */
    Vertex(const glm::vec3 &position, const Color &color, const glm::vec2 &texCoords, const glm::vec3 &normal)
        : position(position), color(color), normal(normal), tex_coord(texCoords) {
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
            bone_ids[i] = 0;
            weights[i] = 0.0f;
        }
    }

    Vertex(const glm::vec3 &position, const Color &color, const glm::vec2 &texCoords, const glm::vec3 &normal,
           const int boneIdsIn[MAX_BONE_INFLUENCE], const float weightsIn[MAX_BONE_INFLUENCE])
        : position(position), color(color), normal(normal), tex_coord(texCoords) {
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
            bone_ids[i] = boneIdsIn[i];
            weights[i] = weightsIn[i];
        }
    }

    // We plan to use this as a key for a map so we need to define the equality operator
    bool operator==(const Vertex &other) const {
        return position == other.position && color == other.color && tex_coord == other.tex_coord &&
               normal == other.normal;
    }
};

} // namespace our

// We plan to use struct Vertex as a key for a map so we need to define a hash function for it
namespace std {
// A Simple method to combine two hash values
inline size_t hash_combine(size_t h1, size_t h2) {
    return h1 ^ (h2 << 1);
}

// A Hash function for struct Vertex
template <> struct hash<our::Vertex> {
    size_t operator()(our::Vertex const &vertex) const {
        size_t combined = hash<glm::vec3>()(vertex.position);
        combined = hash_combine(combined, hash<our::Color>()(vertex.color));
        combined = hash_combine(combined, hash<glm::vec2>()(vertex.tex_coord));
        combined = hash_combine(combined, hash<glm::vec3>()(vertex.normal));
        return combined;
    }
};
} // namespace std
