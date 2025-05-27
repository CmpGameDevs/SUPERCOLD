#pragma once

#include <application.hpp>
#include <components/trail-renderer.hpp>
#include <ecs/entity.hpp>
#include <ecs/world.hpp>
#include <glad/gl.h>
#include <shader/shader.hpp>

namespace our {

class TrailSystem {
    ShaderProgram* trailShader = nullptr;
    GLuint VAO, VBO;

    TrailSystem() = default;
    TrailSystem(const TrailSystem&) = delete;            // Prevent copying
    TrailSystem& operator=(const TrailSystem&) = delete; // Prevent assignment
    TrailSystem(TrailSystem&&) = delete;                 // Prevent moving
    TrailSystem& operator=(TrailSystem&&) = delete;      // Prevent moving assignment

    public:
    static TrailSystem& getInstance() {
        static TrailSystem instance;
        return instance;
    }

    // Initializes shader and VAO/VBO
    void initialize();
    // Processes trail data (adds/removes points) based on deltaTime
    void processTrails(World* world, float deltaTime);
    // Renders all active trails
    void renderTrails(World* world, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
                      const glm::vec3& cameraRight, float bloomBrightnessCutoff);
    // Clean up resources
    void onDestroy();
};

}