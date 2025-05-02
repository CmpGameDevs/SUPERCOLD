#pragma once

#include "../material/material.hpp"
#include "../mesh/mesh.hpp"

namespace our {
class Crosshair {
public:

    static Crosshair* getInstance() {
        static Crosshair instance;
        return &instance;
    }

    Crosshair();
    ~Crosshair();

    bool isVisible() { return visible;}
    void setVisiblity(bool visible) { this->visible = visible; }

    void parseConfig(const nlohmann::json &config);
    void initialize(const nlohmann::json &config);
    void render();

private:
    float lineLength = 0.05f;
    float lineThickness = 0.005f; 
    float gapSize = 0.008f;
    float dotSize = 0.003f;
    bool visible = false;
    our::Mesh* mesh;
    our::TintedMaterial* material;
    glm::vec4 color;
};
}