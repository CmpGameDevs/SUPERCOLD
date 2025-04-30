#pragma once

#include <application.hpp>
#include <asset-loader.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/collision-system.hpp>
#include <systems/weapons-system.hpp>
#include <systems/fps-controller.hpp>
#include <core/time-scale.hpp>
#include <btBulletDynamicsCommon.h>

class PhysicsTestState : public our::State {

    our::World world;
    our::ForwardRenderer renderer;
    our::FPSControllerSystem fpsController;
    our::CollisionSystem &collisionSystem = our::CollisionSystem::getInstance();
    our::WeaponsSystem &weaponsSystem = our::WeaponsSystem::getInstance();
    game::TimeScaler timeScaler;
    float timeScale;

    void onInitialize() override {
        auto& config = getApp()->getConfig()["scene"];

        if(config.contains("assets")){
            our::deserializeAllAssets(config["assets"]);
        }

        // If we have a world in the scene config, we use it to populate our world
        if(config.contains("world")){
            world.deserialize(config["world"]);
        }

        fpsController.enter(getApp());

        glm::ivec2 size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);

        // Create Bullet components
        btDefaultCollisionConfiguration* collisionConfig = new btDefaultCollisionConfiguration();
        btCollisionDispatcher* dispatcher = new btCollisionDispatcher(collisionConfig);
        btBroadphaseInterface* broadphase = new btDbvtBroadphase();
        btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver();

        // Create the dynamics world
        btDiscreteDynamicsWorld* physicsWorld = new btDiscreteDynamicsWorld(
            dispatcher,
            broadphase,
            solver,
            collisionConfig
        );
        collisionSystem.initialize(size, physicsWorld);
        fpsController.setCollisionSystem(&collisionSystem);
        timeScale = 1.0f;
    }

    void raycast() {
        our::Entity* playerEntity = nullptr;
        our::CameraComponent *camera = nullptr;
        for (auto entity : world.getEntities()) {
            if (!camera) camera = entity->getComponent<our::CameraComponent>();
            if (entity->name == "raycast") {
                playerEntity = entity;
                break;
            }
        }

        if (playerEntity) {
            glm::mat4 worldMatrix = playerEntity->getLocalToWorldMatrix();
            glm::vec3 rayStart = glm::vec3(worldMatrix[3]);
            float rayLength = 5.0f;
            
            std::vector<glm::vec3> directions = {
                // Horizontal (XZ plane)
                glm::vec3(1, 0, 0),    // Right
                // glm::vec3(1, 0, 1),    // Right-Forward
                // glm::vec3(0, 0, 1),    // Forward
                // glm::vec3(-1, 0, 1),   // Left-Forward
                glm::vec3(-1, 0, 0),   // Left
                glm::vec3(-1, 0, -1),  // Left-Back
                glm::vec3(0, 0, -1),   // Back
                glm::vec3(1, 0, -1),   // Right-Back
            
                // // Vertical only
                // glm::vec3(0, 1, 0),    // Up
                // glm::vec3(0, -1, 0),   // Down
            
                // // Vertical + horizontal (diagonal in Y as well)
                // glm::vec3(1, 1, 0),    // Up-Right
                // glm::vec3(-1, 1, 0),   // Up-Left
                // glm::vec3(0, 1, 1),    // Up-Forward
                // glm::vec3(0, 1, -1),   // Up-Back
            
                // glm::vec3(1, -1, 0),   // Down-Right
                // glm::vec3(-1, -1, 0),  // Down-Left
                // glm::vec3(0, -1, 1),   // Down-Forward
                // glm::vec3(0, -1, -1)   // Down-Back
            };

            // Apply player's rotation to directions
            glm::quat playerRotation = playerEntity->localTransform.rotation;
            for (auto& dir : directions) {
                dir = glm::normalize(playerRotation * dir);
            }

            for (const auto& direction : directions) {
                glm::vec3 rayEnd = rayStart + direction * rayLength;
                our::CollisionComponent* hitComponent;
                glm::vec3 hitPoint, hitNormal;
                
                bool isHit = collisionSystem.raycast(rayStart, rayEnd, hitComponent, hitPoint, hitNormal);
                if (isHit) {
                    if (hitComponent) {
                        auto hitEntity = hitComponent->getOwner();
                    }
                }
                
                // Draw debug ray
                collisionSystem.debugDrawRay(rayStart, rayEnd, isHit ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 1));
            }

            // Update the player rotation to only face the camera direction
            playerEntity->localTransform.rotation = camera->getOwner()->localTransform.rotation;
        }
    }

    void applyForces() {
        auto keyboard = getApp()->getKeyboard();
        our::Entity* ballEntity = nullptr;
        our::CameraComponent *camera = nullptr;
        for (auto entity : world.getEntities()) {
            if (!camera) camera = entity->getComponent<our::CameraComponent>();
            if (entity->name == "Ball") ballEntity = entity;
        }
        if (!ballEntity) return;

        if (keyboard.isPressed(GLFW_KEY_F)) {
            auto cameraMatrix = camera->getOwner()->getLocalToWorldMatrix();
            glm::vec3 cameraForward = glm::normalize(glm::vec3(cameraMatrix[2]));
            collisionSystem.applyImpulse(ballEntity, cameraForward * 2.0f);
        } else if (keyboard.isPressed(GLFW_KEY_R)) {
            collisionSystem.applyTorque(ballEntity, glm::vec3(0, 10.0f, 0));
        } else if (keyboard.isPressed(GLFW_KEY_T)) {
            glm::vec3 handPosition = camera->getOwner()->localTransform.position + glm::vec3(0, 1.0f, 0);
            glm::vec3 throwDirection = glm::vec3(0.0f, 1.0f, 0.0f);
            glm::vec3 torque = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f)) * 3.0f;
            collisionSystem.applyTorque(ballEntity, torque);
            collisionSystem.applyImpulse(ballEntity, throwDirection, handPosition);
        }
    }

    void onDraw(double deltaTime) override {
        fpsController.update(&world, (float)deltaTime);

        float speed = fpsController.getSpeedMagnitude();
        timeScaler.update(speed);
        timeScale = timeScaler.getTimeScale();
        float scaledDeltaTime = (float)deltaTime;

        applyForces();
        collisionSystem.update(&world, scaledDeltaTime);
        weaponsSystem.update(&world, scaledDeltaTime);
        raycast();
        renderer.render(&world);
        collisionSystem.debugDrawWorld(&world);
    }

    void onDestroy() override {
        // Don't forget to destroy the renderer
        renderer.destroy();
        // Destroy the collision system and free resources
        collisionSystem.destroy();
        // Destroy the weapons system and free resources
        weaponsSystem.onDestroy();
        // On exit, we call exit for the camera controller system to make sure that the mouse is unlocked
        fpsController.exit();
        // Clear the world
        world.clear();
        // and we delete all the loaded assets to free memory on the RAM and the VRAM
        our::clearAllAssets();
    }
};


