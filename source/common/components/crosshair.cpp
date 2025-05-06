#include "Crosshair.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <iostream>

our::Crosshair::Crosshair() : mesh(nullptr), material(nullptr), color(1.0f, 1.0f, 1.0f, 1.0f) {}

our::Crosshair::~Crosshair() {
    if (mesh)
        delete mesh;
    if (material)
        delete material;
}

void our::Crosshair::parseConfig(const nlohmann::json &config) {
    lineLength = config.value("lineLength", lineLength);
    lineThickness = config.value("lineThickness", lineThickness);
    gapSize = config.value("gapSize", gapSize);
    dotSize = config.value("dotSize", dotSize);
    if (config.contains("color") && config["color"].is_array() && config["color"].size() == 4) {
        color = glm::vec4(config["color"][0].get<float>(), config["color"][1].get<float>(),
                          config["color"][2].get<float>(), config["color"][3].get<float>());
    } else {
        color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

void our::Crosshair::initialize(const nlohmann::json &config) {
    parseConfig(config);
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    float aspectRatio = (float)viewport[2] / (float)viewport[3];

    if (aspectRatio > 1.0f) {
        lineLength /= aspectRatio;
        gapSize /= aspectRatio;
    }

    std::vector<our::Vertex> vertices;
    std::vector<GLuint> elements;

    if (material) {
        material->tint = this->color;
    }

    our::Color vertexColor = {static_cast<uint8_t>(color.r * 255), static_cast<uint8_t>(color.g * 255),
                              static_cast<uint8_t>(color.b * 255), static_cast<uint8_t>(color.a * 255)};

    vertices.push_back(our::Vertex{{-dotSize, -dotSize, 0}, vertexColor, {0, 0}, {0, 0, 1}});
    vertices.push_back(our::Vertex{{dotSize, -dotSize, 0}, vertexColor, {1, 0}, {0, 0, 1}});
    vertices.push_back(our::Vertex{{dotSize, dotSize, 0}, vertexColor, {1, 1}, {0, 0, 1}});
    vertices.push_back(our::Vertex{{-dotSize, dotSize, 0}, vertexColor, {0, 1}, {0, 0, 1}});

    GLuint base = 0;
    elements.insert(elements.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    base += 4;

    vertices.push_back(our::Vertex{{gapSize, -lineThickness, 0}, vertexColor, {0, 0}, {0, 0, 1}});
    vertices.push_back(our::Vertex{{gapSize + lineLength, -lineThickness, 0}, vertexColor, {1, 0}, {0, 0, 1}});
    vertices.push_back(our::Vertex{{gapSize + lineLength, lineThickness, 0}, vertexColor, {1, 1}, {0, 0, 1}});
    vertices.push_back(our::Vertex{{gapSize, lineThickness, 0}, vertexColor, {0, 1}, {0, 0, 1}});
    elements.insert(elements.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    base += 4;

    vertices.push_back(our::Vertex{{-gapSize - lineLength, -lineThickness, 0}, vertexColor, {0, 0}, {0, 0, 1}});
    vertices.push_back(our::Vertex{{-gapSize, -lineThickness, 0}, vertexColor, {1, 0}, {0, 0, 1}});
    vertices.push_back(our::Vertex{{-gapSize, lineThickness, 0}, vertexColor, {1, 1}, {0, 0, 1}});
    vertices.push_back(our::Vertex{{-gapSize - lineLength, lineThickness, 0}, vertexColor, {0, 1}, {0, 0, 1}});
    elements.insert(elements.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    base += 4;

    vertices.push_back(our::Vertex{{-lineThickness, gapSize + lineLength, 0}, vertexColor, {0, 0}, {0, 0, 1}});
    vertices.push_back(our::Vertex{{lineThickness, gapSize + lineLength, 0}, vertexColor, {1, 0}, {0, 0, 1}});
    vertices.push_back(our::Vertex{{lineThickness, gapSize, 0}, vertexColor, {1, 1}, {0, 0, 1}});
    vertices.push_back(our::Vertex{{-lineThickness, gapSize, 0}, vertexColor, {0, 1}, {0, 0, 1}});
    elements.insert(elements.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    base += 4;

    vertices.push_back(our::Vertex{{-lineThickness, -gapSize, 0}, vertexColor, {0, 0}, {0, 0, 1}});
    vertices.push_back(our::Vertex{{lineThickness, -gapSize, 0}, vertexColor, {1, 0}, {0, 0, 1}});
    vertices.push_back(our::Vertex{{lineThickness, -gapSize - lineLength, 0}, vertexColor, {1, 1}, {0, 0, 1}});
    vertices.push_back(our::Vertex{{-lineThickness, -gapSize - lineLength, 0}, vertexColor, {0, 1}, {0, 0, 1}});
    elements.insert(elements.end(), {base, base + 1, base + 2, base, base + 2, base + 3});

    mesh = new our::Mesh(vertices, elements);

    our::ShaderProgram *shader = new our::ShaderProgram();
    shader->attach("assets/shaders/tinted.vert", GL_VERTEX_SHADER);
    shader->attach("assets/shaders/tinted.frag", GL_FRAGMENT_SHADER);
    shader->link();

    our::PipelineState pipelineState{};
    pipelineState.depthTesting.enabled = false;
    pipelineState.faceCulling.enabled = false;
    pipelineState.blending.enabled = true;
    pipelineState.blending.equation = GL_FUNC_ADD;
    pipelineState.blending.sourceFactor = GL_SRC_ALPHA;
    pipelineState.blending.destinationFactor = GL_ONE_MINUS_SRC_ALPHA;
    pipelineState.setup();

    material = new our::TintedMaterial();
    material->shader = shader;
    material->pipelineState = pipelineState;
    material->tint = color;
}

void our::Crosshair::render() {
    if (mesh && material) {
        material->setup();

        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        float aspectRatio = (float)viewport[2] / (float)viewport[3];

        glm::mat4 ortho;
        if (aspectRatio > 1.0f) {
            ortho = glm::ortho(-aspectRatio, aspectRatio, -1.0f, 1.0f);
        } else {
            ortho = glm::ortho(-1.0f, 1.0f, -1.0f / aspectRatio, 1.0f / aspectRatio);
        }

        material->shader->set("transform", ortho);
        if (weaponHeld) {
            mesh->draw();
        } else {
            glBindVertexArray(mesh->getVertexArray());
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void *)0);
            glBindVertexArray(0);
        }
    }
}
