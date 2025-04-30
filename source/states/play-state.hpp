#pragma once

#include <asset-loader.hpp>
#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/collision-system.hpp>
#include <systems/fps-controller.hpp>
#include <core/time-scale.hpp>

// This state shows how to use the ECS framework and deserialization.
class Playstate : public our::State {
    static bool initialized;
    our::World world;
    our::CollisionSystem &collisionSystem = our::CollisionSystem::getInstance();
    our::ForwardRenderer &renderer = our::ForwardRenderer::getInstance();  
    our::FPSControllerSystem fpsController;
    game::TimeScaler timeScaler;
    float timeScale;

    void initializeGame() {
        if (initialized) return; 
        initialized = true;
        // First of all, we get the scene configuration from the app config
        auto &config = getApp()->getConfig()["scene"];
        // If we have assets in the scene config, we deserialize them
        if (config.contains("assets")) {
            our::deserializeAllAssets(config["assets"]);
        }
        // Then we initialize the renderer
        auto size = getApp()->getFrameBufferSize();
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

    void onInitialize() override {
        // If we have a world in the scene config, we use it to populate our world
        initializeGame();
        // Level-dependant components
        auto &levelConfig = getApp()->getLevelConfig();
        if (levelConfig.contains("world")) {
            world.deserialize(levelConfig["world"]);
        }
        fpsController.enter(getApp());
        fpsController.setCollisionSystem(&collisionSystem);
        timeScale = 1.0f;
    }

    void onDraw(double deltaTime) override {
        // Here, we just run a bunch of systems to control the world logic
        fpsController.update(&world, (float)deltaTime);
        
        float speed = fpsController.getSpeedMagnitude();
        
        timeScaler.update(speed);

        timeScale = timeScaler.getTimeScale();

        float scaledDeltaTime = (float)deltaTime * timeScale;

        collisionSystem.update(&world, scaledDeltaTime);
        // // And finally we use the renderer system to draw the scene
        renderer.render(&world);

        // Get a reference to the keyboard object
        auto &keyboard = getApp()->getKeyboard();

        if (keyboard.justPressed(GLFW_KEY_ESCAPE)) {
            // If the escape key is pressed in this frame, go to the play state
            getApp()->goToNextLevel();
        }
    }

    void onDestroy() override {
        //this has to be uncommented to prevent memory leaks, but it will cause a crash in collision system
        // world.clear();
    }
};

bool Playstate::initialized = false;
