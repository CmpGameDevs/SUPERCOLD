#pragma once

#include <application.hpp>
#include <asset-loader.hpp>
#include <systems/free-camera-controller.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/collision-system.hpp>
#include <btBulletDynamicsCommon.h>

class PhysicsTestState : public our::State {

    our::World world;
    our::ForwardRenderer renderer;
    our::FreeCameraControllerSystem cameraController;

    our::CollisionSystem collisionSystem;

    void onInitialize() override {
        auto& config = getApp()->getConfig()["scene"];

        if(config.contains("assets")){
            our::deserializeAllAssets(config["assets"]);
        }

        // If we have a world in the scene config, we use it to populate our world
        if(config.contains("world")){
            world.deserialize(config["world"]);
        }

        cameraController.enter(getApp());

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
    }

    void raycast() {
        our::Entity* playerEntity = nullptr;
        for (auto entity : world.getEntities()) {
            if (entity->name == "raycast") {
                playerEntity = entity;
                break;
            }
        }

        if (playerEntity) {
            glm::vec3 rayStart = playerEntity->localTransform.position;
            float rayLength = 5.0f;
            
            std::vector<glm::vec3> directions = {
                // Horizontal (XZ plane)
                glm::vec3(1, 0, 0),    // Right
                glm::vec3(1, 0, 1),    // Right-Forward
                glm::vec3(0, 0, 1),    // Forward
                glm::vec3(-1, 0, 1),   // Left-Forward
                glm::vec3(-1, 0, 0),   // Left
                glm::vec3(-1, 0, -1),  // Left-Back
                glm::vec3(0, 0, -1),   // Back
                glm::vec3(1, 0, -1),   // Right-Back
            
                // Vertical only
                glm::vec3(0, 1, 0),    // Up
                glm::vec3(0, -1, 0),   // Down
            
                // Vertical + horizontal (diagonal in Y as well)
                glm::vec3(1, 1, 0),    // Up-Right
                glm::vec3(-1, 1, 0),   // Up-Left
                glm::vec3(0, 1, 1),    // Up-Forward
                glm::vec3(0, 1, -1),   // Up-Back
            
                glm::vec3(1, -1, 0),   // Down-Right
                glm::vec3(-1, -1, 0),  // Down-Left
                glm::vec3(0, -1, 1),   // Down-Forward
                glm::vec3(0, -1, -1)   // Down-Back
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
        }
    }

    void onDraw(double deltaTime) override {
        cameraController.update(&world, (float)deltaTime);
        collisionSystem.update(&world, (float)deltaTime);
        raycast();
        renderer.render(&world);
        collisionSystem.debugDrawWorld(&world);
    }

    void onDestroy() override {
        // Don't forget to destroy the renderer
        renderer.destroy();
        // Destroy the collision system and free resources
        collisionSystem.destroy();
        // On exit, we call exit for the camera controller system to make sure that the mouse is unlocked
        cameraController.exit();
        // Clear the world
        world.clear();
        // and we delete all the loaded assets to free memory on the RAM and the VRAM
        our::clearAllAssets();
    }
};


