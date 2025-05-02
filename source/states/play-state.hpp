#pragma once

#include <asset-loader.hpp>
#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/collision-system.hpp>
#include <systems/fps-controller.hpp>
#include <core/time-scale.hpp>
#include <systems/text-renderer.hpp>


class Playstate : public our::State {
    static bool initialized;
    our::World world;
    our::CollisionSystem& collisionSystem = our::CollisionSystem::getInstance();
    our::ForwardRenderer& renderer = our::ForwardRenderer::getInstance();
    our::FPSControllerSystem fpsController;
    game::TimeScaler timeScaler;
    float timeScale;
    our::TextRenderer& textRenderer = our::TextRenderer::getInstance();  

    void initializeGame() {
        if (initialized) return; 
        initialized = true;
        
        // Retrieve scene configuration from the app config
        auto& config = getApp()->getConfig()["scene"];
        
        // Deserialize all assets from the configuration if present
        if (config.contains("assets")) {
            our::deserializeAllAssets(config["assets"]);
        }

        // Initialize the renderer with the appropriate configuration
        auto size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);

        // Initialize Bullet physics system
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

        // Initialize the collision system with the physics world
        collisionSystem.initialize(size, physicsWorld);
        auto windowSize = getApp()->getWindowSize();
        textRenderer.initialize(windowSize.x, windowSize.y);  
    }

    void onInitialize() override {
        initializeGame();

        // Level-dependent components initialization
        auto& levelConfig = getApp()->getLevelConfig();
        
        if (levelConfig.contains("world")) {
            world.deserialize(levelConfig["world"]);
        }
        textRenderer.showCenteredText("SUPER");
        textRenderer.showCenteredText("COLD");

        // Set up the FPS controller system and assign the collision system
        fpsController.enter(getApp());
        
        // Set default time scale
        timeScale = 1.0f;
    }

    void onDraw(double deltaTime) override {
        // Update FPS Controller
        fpsController.update(&world, (float)deltaTime);
        
        // Get speed magnitude from FPS controller
        float speed = fpsController.getSpeedMagnitude();

        // Update the time scaler based on the speed
        timeScaler.update(speed);

        // Get the current time scale
        timeScale = timeScaler.getTimeScale();

        // Apply the time scale to the delta time
        float scaledDeltaTime = (float)deltaTime * timeScale;

        // Update the collision system
        collisionSystem.update(&world, scaledDeltaTime);

        // Render the world using the renderer system
        renderer.render(&world);

        // Render some test text
        textRenderer.renderCenteredText();

        // Handle keyboard input (escape key to transition between levels)
        auto& keyboard = getApp()->getKeyboard();
        
        if (keyboard.justPressed(GLFW_KEY_ESCAPE)) {
            getApp()->goToNextLevel();
        }
    }

    void onDestroy() override {
        // Uncomment to clean up the world and prevent memory leaks, but ensure collision system is intact
        world.clear();
    }
};

// Static member initialization
bool Playstate::initialized = false;