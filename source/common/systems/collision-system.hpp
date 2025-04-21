#pragma once
#include <vector>
#include <functional>
#include <btBulletDynamicsCommon.h>
#include <LinearMath/btIDebugDraw.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/glm.hpp>
#include <glad/gl.h>
#include <ecs/entity.hpp>
#include <ecs/world.hpp>
#include <shader/shader.hpp>
#include <components/collision.hpp>
#include <components/camera.hpp>

namespace our {

class CollisionSystem;  // Forward declaration of CollisionSystem

class GLDebugDrawer : public btIDebugDraw {
    glm::ivec2 windowSize;
    int m_debugMode;
    struct Line {
        glm::vec3 start, end;
        glm::vec3 color;
    };
    std::vector<Line> lines;
    GLuint VAO, VBO;
    ShaderProgram* lineShader = nullptr;
public:
    GLDebugDrawer(glm::ivec2 windowSize) : m_debugMode(DBG_DrawWireframe | DBG_DrawAabb) {
        this->windowSize = windowSize;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        lineShader = new ShaderProgram();
        lineShader->attach("assets/shaders/debug_line.vert", GL_VERTEX_SHADER);
        lineShader->attach("assets/shaders/debug_line.frag", GL_FRAGMENT_SHADER);
        lineShader->link();
    }

    ~GLDebugDrawer() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        delete lineShader;
    }

    void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override {
        lines.push_back({
            {from.x(), from.y(), from.z()},
            {to.x(), to.y(), to.z()},
            {color.x(), color.y(), color.z()}
        });
    }

    void flushLines(World *world) {
        CameraComponent *camera = nullptr;
        for (auto entity : world->getEntities()) {
            if (!camera)
                camera = entity->getComponent<CameraComponent>();
        }

        if(lines.empty() || camera == nullptr) return;
        glm::mat4 view = camera->getViewMatrix();
        glm::mat4 projection = camera->getProjectionMatrix(windowSize);
        glm::mat4 VP = projection * view;

        // Prepare data
        std::vector<float> vertexData;
        for(const auto& line : lines) {
            vertexData.insert(vertexData.end(), {line.start.x, line.start.y, line.start.z});
            vertexData.insert(vertexData.end(), {line.color.r, line.color.g, line.color.b});
            
            vertexData.insert(vertexData.end(), {line.end.x, line.end.y, line.end.z});
            vertexData.insert(vertexData.end(), {line.color.r, line.color.g, line.color.b});
        }

        // Upload to GPU
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_DYNAMIC_DRAW);

        // Set vertex attributes
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0); // Position
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float))); // Color

        
        // Draw
        lineShader->use();
        lineShader->set("viewProjMatrix", VP);
        glDrawArrays(GL_LINES, 0, lines.size() * 2);

        // Cleanup
        glBindVertexArray(0);
        lines.clear();
    }

    void setDebugMode(int debugMode) override { m_debugMode = debugMode; }
    int getDebugMode() const override { return m_debugMode; }

    // Stubs for unused methods
    virtual void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color) override {}
    void reportErrorWarning(const char* warningString) override {}
    void draw3dText(const btVector3& location, const char* textString) override {}
};

class CollisionSystem {
    btDiscreteDynamicsWorld* physicsWorld = nullptr;
    GLDebugDrawer* debugDrawer = nullptr;

    // Step the physics simulation by a given time step
    void _stepSimulation(float deltaTime);
    // Process all entities in the world and update their transforms and physics bodies
    void _processEntities(World* world);
    // Sync the transforms of the entity and its collision component
    void _syncTransforms(Entity* entity, CollisionComponent* collision, Transform* transform);
    // Create a rigid body for the entity based on its collision component and transform
    void _createRigidBody(Entity* entity, CollisionComponent* collision, const Transform* transform);
    // Clear previous collisions for the entity
    void _clearPreviousCollisions(World* world);
    // Detect collisions between entities and update their collision components
    void _detectCollisions();

public:
    // Initialize the collision system with a Bullet physics world
    void initialize(glm::ivec2 windowSize, btDynamicsWorld* physicsWorld);

    // Get the Bullet physics world
    btDiscreteDynamicsWorld* getPhysicsWorld() { return physicsWorld; }

    // Update entity transforms and physics simulation
    void update(World* world, float deltaTime);

    // Perform a raycast and return hit results
    bool raycast(const glm::vec3& start, const glm::vec3& end, 
        CollisionComponent*& hitComponent, glm::vec3& hitPoint, glm::vec3& hitNormal);

    // Apply an instant impulse (e.g., for throwing)
    void applyImpulse(Entity* entity, const glm::vec3& force, const glm::vec3& position = glm::vec3(0));
    
    // Apply continuous force (e.g., wind, thrusters)
    void applyForce(Entity* entity, const glm::vec3& force, const glm::vec3& position = glm::vec3(0));
    
    // Apply torque for rotation (e.g., spinning objects)
    void applyTorque(Entity* entity, const glm::vec3& torque);

    // Debug draw the world using the debug drawer
    void debugDrawWorld(World* world);

    // Debug-draw a ray (call this after raycast if you want to visualize)
    void debugDrawRay(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color);

    // Destroy the collision system and free resources
    void destroy();
};

} // namespace our