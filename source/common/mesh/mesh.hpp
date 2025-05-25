#pragma once

#include "vertex.hpp"
#include <glad/gl.h>

namespace our {

#define ATTRIB_LOC_POSITION 0
#define ATTRIB_LOC_COLOR 1
#define ATTRIB_LOC_TEXCOORD 2
#define ATTRIB_LOC_NORMAL 3
#define ATTRIB_LOC_BONE_IDS 4
#define ATTRIB_LOC_WEIGHTS 5

class Mesh {
    // Here, we store the object names of the 3 main components of a mesh:
    // A vertex array object, A vertex buffer and an element buffer
    unsigned int VBO, EBO;
    unsigned int VAO;
    // We need to remember the number of elements that will be draw by glDrawElements
    GLsizei elementCount;

    void setupBuffers(const std::vector<Vertex> &vertices, const std::vector<unsigned int> &elements) {
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements.size() * sizeof(unsigned int), elements.data(), GL_STATIC_DRAW);
    }

    void setupAttributes() {

        // 0. Position attribute (glm::vec3)
        glEnableVertexAttribArray(ATTRIB_LOC_POSITION);
        glVertexAttribPointer(ATTRIB_LOC_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void *)offsetof(Vertex, position));

        // 1. Color attribute (our::Color)
        // NOTE: GL_TRUE means that the color is normalized to [0,1] range
        glEnableVertexAttribArray(ATTRIB_LOC_COLOR);
        glVertexAttribPointer(ATTRIB_LOC_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex),
                              (void *)offsetof(Vertex, color));

        // 2. Texture coordinate attribute (glm::vec2)
        glEnableVertexAttribArray(ATTRIB_LOC_TEXCOORD);
        glVertexAttribPointer(ATTRIB_LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void *)offsetof(Vertex, tex_coord));

        // 3. Normal attribute (glm::vec3)
        glEnableVertexAttribArray(ATTRIB_LOC_NORMAL);
        glVertexAttribPointer(ATTRIB_LOC_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void *)offsetof(Vertex, normal));

        // 4. Bone IDs attribute (int[MAX_BONE_INFLUENCE])
        // NOTE: for int I used glVertexAttrib`I`Pointer
        glEnableVertexAttribArray(ATTRIB_LOC_BONE_IDS);
        glVertexAttribIPointer(ATTRIB_LOC_BONE_IDS, MAX_BONE_INFLUENCE, GL_INT, sizeof(Vertex),
                               (void *)offsetof(Vertex, bone_ids));

        // 5. Weights attribute (int[MAX_BONE_INFLUENCE])
        glEnableVertexAttribArray(ATTRIB_LOC_WEIGHTS);
        glVertexAttribPointer(ATTRIB_LOC_WEIGHTS, MAX_BONE_INFLUENCE, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void *)offsetof(Vertex, weights));
    }

  public:
    // Add CPU-side storage
    std::vector<Vertex> cpuVertices;
    std::vector<unsigned int> cpuIndices;

    // The constructor takes two vectors:
    // - vertices which contain the vertex data.
    // - elements which contain the indices of the vertices out of which each rectangle will be constructed.
    // The mesh class does not keep a these data on the RAM. Instead, it should create
    // a vertex buffer to store the vertex data on the VRAM,
    // an element buffer to store the element data on the VRAM,
    // a vertex array object to define how to read the vertex & element buffer during rendering
    Mesh(const std::vector<Vertex> &vertices, const std::vector<unsigned int> &elements)
        : cpuVertices(vertices), cpuIndices(elements) {
        // TODO: (Req 2) Write this function
        //  remember to store the number of elements in "elementCount" since you will need it for drawing
        //  For the attribute locations, use the constants defined above: ATTRIB_LOC_POSITION, ATTRIB_LOC_COLOR, etc

        elementCount = static_cast<GLsizei>(elements.size());

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        setupBuffers(vertices, elements);
        setupAttributes();
        glBindVertexArray(0);
    }

    // Get the vertex array object of the mesh
    unsigned int getVertexArray() const { return VAO; }

    // this function should render the mesh
    void draw() {
        // TODO: (Req 2) Write this function
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, elementCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    // this function should delete the vertex & element buffers and the vertex array object
    ~Mesh() {
        // TODO: (Req 2) Write this function
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }

    Mesh(Mesh &&other) noexcept { *this = std::move(other); }

    Mesh &operator=(Mesh &&other) noexcept {
        if (this != &other) {
            std::swap(VAO, other.VAO);
            std::swap(VBO, other.VBO);
            std::swap(EBO, other.EBO);
            std::swap(elementCount, other.elementCount);
            std::swap(cpuVertices, other.cpuVertices);
            std::swap(cpuIndices, other.cpuIndices);
        }
        return *this;
    }
};

} // namespace our
