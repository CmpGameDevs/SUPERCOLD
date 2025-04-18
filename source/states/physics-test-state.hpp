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

    void onDraw(double deltaTime) override {
        cameraController.update(&world, (float)deltaTime);
        collisionSystem.stepSimulation((float)deltaTime);
        collisionSystem.update(&world, (float)deltaTime);
        renderer.render(&world);
        // Draw collision wireframes
        collisionSystem.getPhysicsWorld()->debugDrawWorld();
        collisionSystem.getDebugDrawer()->flushLines(&world);
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


