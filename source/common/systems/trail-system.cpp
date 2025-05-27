#include "trail-system.hpp"
#include <components/trail-renderer.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GL/gl.h>

namespace our {

void TrailSystem::initialize() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    trailShader = new ShaderProgram();
    trailShader->attach("assets/shaders/trail.vert", GL_VERTEX_SHADER);
    trailShader->attach("assets/shaders/trail.frag", GL_FRAGMENT_SHADER);
    trailShader->link();
}

void TrailSystem::processTrails(World *world, float deltaTime) {
    for (auto entity : world->getEntities()) {
        TrailRenderer *trailComponent = entity->getComponent<TrailRenderer>();
        Transform *transform = &entity->localTransform;

        if (trailComponent && transform) {
            trailComponent->timeSinceLastPoint += deltaTime;
            if (trailComponent->timeSinceLastPoint >= trailComponent->pointAddInterval) {
                trailComponent->trailPoints.push_back(transform->toMat4()[3]);
                trailComponent->timeSinceLastPoint = 0.0f;

                while (trailComponent->trailPoints.size() > trailComponent->maxTrailPoints) {
                    trailComponent->trailPoints.erase(trailComponent->trailPoints.begin());
                }
            }
        }
    }
}

void TrailSystem::renderTrails(World *world, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraRight, float bloomBrightnessCutoff) {
    trailShader->use();
    trailShader->set("projection", projectionMatrix);
    trailShader->set("view", viewMatrix);
    trailShader->set("bloomBrightnessCutoff", bloomBrightnessCutoff);

    // Bind VAO and VBO once for all trails
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Configure vertex attributes once for all trails (will reuse for each draw call)
    glEnableVertexAttribArray(0); // Position attribute (layout 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); // Segment Ratio attribute (layout 1)
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));

    // Iterate through all entities to find trails and render them
    for (auto entity : world->getEntities()) {
        TrailRenderer *trailComponent = entity->getComponent<TrailRenderer>();
        if (trailComponent && trailComponent->trailPoints.size() >= 2) {
            // Prepare vertex data for the trail (quad strip)
            std::vector<float> vertexData;
            vertexData.reserve(trailComponent->trailPoints.size() * 4);

            for (size_t i = 0; i < trailComponent->trailPoints.size(); ++i) {
                glm::vec3 p0 = trailComponent->trailPoints[i];
                float t = 1.0f - (float)i / (trailComponent->trailPoints.size() - 1); // Normalized position along trail (0 to 1)

                if (i < trailComponent->trailPoints.size() - 1) {
                    glm::vec3 p1 = trailComponent->trailPoints[i+1];
                    glm::vec3 segmentDir = glm::normalize(p1 - p0);

                    glm::vec3 normal = glm::normalize(glm::cross(segmentDir, cameraRight));

                    float currentWidth = glm::mix(trailComponent->startWidth, trailComponent->endWidth, t);

                    glm::vec3 v0_offset = normal * currentWidth * 0.5f;
                    glm::vec3 v1_offset = normal * currentWidth * -0.5f;

                    vertexData.push_back((p0 + v0_offset).x);
                    vertexData.push_back((p0 + v0_offset).y);
                    vertexData.push_back((p0 + v0_offset).z);
                    vertexData.push_back(t);

                    vertexData.push_back((p0 + v1_offset).x);
                    vertexData.push_back((p0 + v1_offset).y);
                    vertexData.push_back((p0 + v1_offset).z);
                    vertexData.push_back(t);

                    if (i == trailComponent->trailPoints.size() - 2) {
                        float next_t = 1.0f - (float)(i+1) / (trailComponent->trailPoints.size() - 1);
                        float nextWidth = glm::mix(trailComponent->startWidth, trailComponent->endWidth, next_t);
                        glm::vec3 v2_offset = normal * nextWidth * 0.5f;
                        glm::vec3 v3_offset = normal * nextWidth * -0.5f;

                        vertexData.push_back((p1 + v2_offset).x);
                        vertexData.push_back((p1 + v2_offset).y);
                        vertexData.push_back((p1 + v2_offset).z);
                        vertexData.push_back(next_t);

                        vertexData.push_back((p1 + v3_offset).x);
                        vertexData.push_back((p1 + v3_offset).y);
                        vertexData.push_back((p1 + v3_offset).z);
                        vertexData.push_back(next_t);
                    }
                }
            }


            glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_DYNAMIC_DRAW);

            trailShader->set("model", glm::mat4(1.0f));
            trailShader->set("trailColor", trailComponent->trailColor);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, vertexData.size() / 4);
        }
    }

    // Unbind VBO and VAO after all trails are drawn
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void TrailSystem::onDestroy() {
    // Cleanup VAO and VBO
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    // Delete shader
    if (trailShader) {
        delete trailShader;
        trailShader = nullptr;
    }
}

}