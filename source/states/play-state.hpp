#pragma once

#include <asset-loader.hpp>
#include <components/crosshair.hpp>
#include <core/time-scale.hpp>
#include <ecs/world.hpp>
#include <systems/audio-system.hpp>
#include <systems/collision-system.hpp>
#include <systems/enemy-system.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/fps-controller.hpp>
#include <systems/movement.hpp>
#include <systems/text-renderer.hpp>

class Playstate : public our::State {
    static bool initialized;
    our::World world;
    our::CollisionSystem &collisionSystem = our::CollisionSystem::getInstance();
    our::ForwardRenderer &renderer = our::ForwardRenderer::getInstance();
    our::WeaponsSystem &weaponsSystem = our::WeaponsSystem::getInstance();
    our::MovementSystem movementSystem;
    our::FPSControllerSystem fpsController;
    game::TimeScaler timeScaler;
    float timeScale;
    our::TextRenderer &textRenderer = our::TextRenderer::getInstance();
    our::AudioSystem &audioSystem = our::AudioSystem::getInstance();
    our::EnemySystem &enemySystem = our::EnemySystem::getInstance();
    bool gameEnded = false;

    void initializeGame() {
        // Only initialize the game one time
        if (initialized)
            return;

        initialized = true;

        // Retrieve scene configuration from the app config
        auto &config = getApp()->getConfig()["scene"];

        // Deserialize all assets from the configuration if present
        if (config.contains("assets")) {
            our::deserializeAllAssets(config["assets"]);
        }

        // Initialize the renderer with the appropriate configuration
        auto size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);

        // Initialize Bullet physics system
        btDefaultCollisionConfiguration *collisionConfig = new btDefaultCollisionConfiguration();
        btCollisionDispatcher *dispatcher = new btCollisionDispatcher(collisionConfig);
        btBroadphaseInterface *broadphase = new btDbvtBroadphase();
        btSequentialImpulseConstraintSolver *solver = new btSequentialImpulseConstraintSolver();

        // Create the dynamics world
        btDiscreteDynamicsWorld *physicsWorld =
            new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfig);

        // Initialize the collision system with the physics world
        collisionSystem.initialize(size, physicsWorld);
        auto windowSize = getApp()->getWindowSize();
        textRenderer.initialize(windowSize.x, windowSize.y);

        audioSystem.initialize(getApp()->getAudioContext());
        enemySystem.setCollisionSystem();
    }

    void onInitialize() override {
        initializeGame();

        auto &levelConfig = getApp()->getLevelConfig();

        if (levelConfig.contains("world")) {
            world.deserialize(levelConfig["world"]);
        }

        textRenderer.showCenteredText("SUPER", "game");
        textRenderer.showCenteredText("COLD", "game");

        fpsController.enter(getApp());

        timeScale = 1.0f;
    }

    void handleGameEnd() {
        // Handle winning the game
        if (enemySystem.getEnemyCount() == 0 && !gameEnded) {
            textRenderer.showCenteredText("SUPER", "game");
            textRenderer.showCenteredText("HOT", "game");
            textRenderer.showCenteredText("SUPER", "game");
            textRenderer.showCenteredText("HOT", "game");
            textRenderer.showCenteredText("SUPER", "game");
            textRenderer.showCenteredText("HOT", "game");
            audioSystem.playSfx("SUPERHOT", false, 8.0f);
            gameEnded = true;
        }

        if (gameEnded) {
            auto windowSize = getApp()->getWindowSize();
            textRenderer.renderText("Press ENTER to continue", "game", windowSize.x / 2.0f - 150.0f, 50.0f,
                                    0.011f, glm::vec4(1.0f));
        }
    }

    void onDraw(double deltaTime) override {
        std::string backgroundTrack = "level_" + std::to_string(getApp()->getLevelIndex());
        audioSystem.playBackgroundMusic(backgroundTrack, 0.2f, "music");

        // Get speed magnitude from FPS controller
        float speed = fpsController.getSpeedMagnitude();

        // Update the time scaler based on the speed
        timeScaler.update(speed);

        // Get the current time scale
        timeScale = timeScaler.getTimeScale();

        // Apply the time scale to the delta time
        float scaledDeltaTime = (float)deltaTime * (!gameEnded ? timeScale : 1.0f);

        // Update the audio system
        audioSystem.update(&world, scaledDeltaTime);

        // Update the movement system
        movementSystem.update(&world, scaledDeltaTime);

        // Update the collision system
        collisionSystem.update(&world, scaledDeltaTime);

        // Update the weapons system
        weaponsSystem.update(&world, scaledDeltaTime);

        // Update the enemies system
        enemySystem.update(&world, scaledDeltaTime);

        // Render the world using the renderer system
        if (renderer.postprocess) {
            float timeStill = fpsController.getTimeStandingStill();
            float targetIntensity;
            float vignetteIntensity = renderer.postprocess->getEffectParameter("vignetteIntensity");

            if (timeStill > 0.0f) {
                // Linearly interpolate between 0 and 0.6 over 15 seconds
                targetIntensity = std::min((timeStill - 5) / 15.0f, 1.0f) * 0.6f;
            } else {
                // Player moved again, fade out
                targetIntensity = 0.0f;
            }

            float lerpSpeed = 5.0f * deltaTime; // deltaTime is frame time in seconds
            vignetteIntensity += (targetIntensity - vignetteIntensity) * lerpSpeed;

            renderer.postprocess->setEffectParameter("vignetteIntensity", vignetteIntensity);
        }
        renderer.render(&world);

        // Debug draw the collision world
        collisionSystem.debugDrawWorld(&world);

        textRenderer.renderCenteredText();

        // Handle game ending
        handleGameEnd();

        fpsController.update(&world, scaledDeltaTime);
        // Handle keyboard input (escape key to transition between levels)
        auto &keyboard = getApp()->getKeyboard();

        if (gameEnded && keyboard.justPressed(GLFW_KEY_ENTER)) {
            textRenderer.clearTextQueue();
            getApp()->goToNextLevel();
        } else if (keyboard.justPressed(GLFW_KEY_L)) {
            collisionSystem.toggleDebugMode();
        } else if (keyboard.justPressed(GLFW_KEY_ESCAPE)) {
            textRenderer.clearTextQueue();
            getApp()->changeState("menu");
        }
    }

    void onDestroy() override {
        fpsController.turnOffCrosshair();
        world.clear();
    }
};

// Static member initialization
bool Playstate::initialized = false;