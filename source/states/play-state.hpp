#pragma once

#include <application.hpp>

#include <asset-loader.hpp>
#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/collision-system.hpp>
#include <systems/fps-controller.hpp>
#include <systems/movement.hpp>
#include <core/time-scale.hpp>

// This state shows how to use the ECS framework and deserialization.
class Playstate : public our::State {

    our::World world;
    our::ForwardRenderer renderer;
    our::CollisionSystem &collisionSystem = our::CollisionSystem::getInstance();
    our::FPSControllerSystem fpsController;
    our::MovementSystem movementSystem;
    game::TimeScaler timeScaler;
    float timeScale;

    void onInitialize() override {
        // First of all, we get the scene configuration from the app config
        auto &config = getApp()->getConfig()["scene"];
        // If we have assets in the scene config, we deserialize them
        if (config.contains("assets")) {
            our::deserializeAllAssets(config["assets"]);
        }
        // If we have a world in the scene config, we use it to populate our world
        if (config.contains("world")) {
            world.deserialize(config["world"]);
        }
        // We initialize the camera controller system since it needs a pointer to the app
        fpsController.enter(getApp());
        // Then we initialize the renderer
        auto size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);

        timeScale = 1.0f;

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
    }

    void onDraw(double deltaTime) override {
        // Here, we just run a bunch of systems to control the world logic
        fpsController.update(&world, (float)deltaTime);

        float speed = fpsController.getSpeedMagnitude();
        timeScaler.update(speed);
        timeScale = timeScaler.getTimeScale();

        float scaledDeltaTime = (float)deltaTime * timeScale;

        movementSystem.update(&world, scaledDeltaTime);
        collisionSystem.update(&world, scaledDeltaTime);
        // And finally we use the renderer system to draw the scene
        renderer.render(&world);
        collisionSystem.debugDrawWorld(&world);

        // Get a reference to the keyboard object
        auto &keyboard = getApp()->getKeyboard();

        if (keyboard.justPressed(GLFW_KEY_ESCAPE)) {
            // If the escape key is pressed in this frame, go to the play state
            getApp()->changeState("menu");
        }
    }

    void onDestroy() override {
        // Don't forget to destroy the renderer
        renderer.destroy();
        // On exit, we call exit for the camera controller system to make sure that the mouse is unlocked
        fpsController.exit();
        // Clear the world
        world.clear();
        // and we delete all the loaded assets to free memory on the RAM and the VRAM
        our::clearAllAssets();
    }
};
