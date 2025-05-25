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
#include <settings.hpp>
#include "imgui.h"

class Playstate : public our::State {
    static bool initialized;
    our::World world;
    our::CollisionSystem& collisionSystem = our::CollisionSystem::getInstance();
    our::ForwardRenderer& renderer = our::ForwardRenderer::getInstance();
    our::WeaponsSystem& weaponsSystem = our::WeaponsSystem::getInstance();
    our::MovementSystem movementSystem;
    our::FPSControllerSystem fpsController;
    game::TimeScaler timeScaler;
    float timeScale;
    our::TextRenderer& textRenderer = our::TextRenderer::getInstance();
    our::AudioSystem& audioSystem = our::AudioSystem::getInstance();
    our::EnemySystem& enemySystem = our::EnemySystem::getInstance();
    bool gameEnded = false;

    void initializeGame() {
        // Only initialize the game one time
        if (initialized)
            return;

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
        btDiscreteDynamicsWorld* physicsWorld =
            new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfig);

        // Initialize the collision system with the physics world
        collisionSystem.initialize(size, physicsWorld);
        auto windowSize = getApp()->getWindowSize();
        textRenderer.initialize(windowSize.x, windowSize.y);

        audioSystem.initialize(getApp()->getAudioContext());
    }

    void onInitialize() override {
        initializeGame();

        auto& levelConfig = getApp()->getLevelConfig();

        if (levelConfig.contains("world")) {
            world.deserialize(levelConfig["world"]);
        }

        textRenderer.showCenteredText("SUPER", "game");
        textRenderer.showCenteredText("COLD", "game");

        fpsController.enter(getApp());
        enemySystem.setCollisionSystem();

        timeScale = 1.0f;
        gameEnded = false;
    }

    void onImmediateGui() {
        // Shader Debugger
        Settings& settings = Settings::getInstance();
        if (settings.showImGuiShaderDebugMenu) {

            ImGui::ShowMetricsWindow();

            ImGui::Begin("Model Shader Debugger");

            // Select shader mode using a drop down select
            const char* shaderModes[] = {"none",
                "normal",
                "albedo", 
                "depth",
                "metallic",
                "roughness",
                "emission",
                "ambient",
                "metailic_roughness",
                "wireframe",
                "texture_coordinates"
            };

            if (ImGui::BeginCombo("Debug View Mode", settings.shaderDebugMode.c_str())) {
                for (const auto& mode : shaderModes) {
                    bool isSelected = (mode == settings.shaderDebugMode);
                    if (ImGui::Selectable(mode, isSelected)) {
                        settings.shaderDebugMode = mode;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Text("Press F9 to close this window");

            ImGui::End();
        }
    }

    void handleGameEnd() {
        // Handle winning the game
        if (fpsController.isPlayerDead() && !gameEnded) {
            gameEnded = true;
        } else if (enemySystem.getEnemyCount() == 0 && !gameEnded) {
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
            std::string text = fpsController.isPlayerDead() ? "Press R to restart" : "Press ENTER to continue";
            textRenderer.renderText(text, "game", windowSize.x / 2.0f - 150.0f, 50.0f, 0.011f, glm::vec4(1.0f));
        }
    }

    void onDraw(double deltaTime) override {
        std::string backgroundTrack = "level_" + std::to_string(getApp()->getLevelIndex() % 3 + 1);
        audioSystem.playBackgroundMusic(backgroundTrack, 0.2f, "music");

        // Get speed magnitude from FPS controller
        float speed = fpsController.getSpeedMagnitude();

        // Update the time scaler based on the speed
        timeScaler.updateWorldTimeScale(speed);

        // Get the current time scale
        timeScale = timeScaler.getWorldTimeScale();

        bool levelPassed = !fpsController.isPlayerDead() && enemySystem.getEnemyCount() == 0;
        bool levelFailed = fpsController.isPlayerDead() && gameEnded;

        // Apply the time scale to the delta time
        float scaledDeltaTime = (float)deltaTime * (!gameEnded ? timeScale : 1.0f);

        // Update the audio system
        audioSystem.update(&world, scaledDeltaTime);

        // Update the collision system
        collisionSystem.update(&world, scaledDeltaTime);

        float playerDeltaTime = deltaTime;
        if (!levelFailed) {
            // Update the movement system
            movementSystem.update(&world, scaledDeltaTime);

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

                // Update the FPS controller with the current time scale
                timeScaler.updatePlayerTimeScale(vignetteIntensity);

                // Get the current player time scale
                float playerTimeScale = timeScaler.getPlayerTimeScale();

                playerDeltaTime = deltaTime * playerTimeScale;

                renderer.postprocess->setEffectParameter("vignetteIntensity", vignetteIntensity);
            }
        }

        renderer.render(&world);

        // Debug draw the collision world
        collisionSystem.debugDrawWorld(&world);

        textRenderer.renderCenteredText();

        // Handle game ending
        handleGameEnd();

        fpsController.update(&world, (float)playerDeltaTime, (float)deltaTime);

        // Handle keyboard input (escape key to transition between levels)
        auto& keyboard = getApp()->getKeyboard();

        if (gameEnded && levelPassed && keyboard.justPressed(GLFW_KEY_ENTER)) {
            audioSystem.stopSfx("SUPERHOT");
            textRenderer.clearTextQueue();
            getApp()->goToNextLevel();
        } else if (gameEnded && !levelPassed && keyboard.justPressed(GLFW_KEY_R)) {
            textRenderer.clearTextQueue();
            int currentLevelIndex = getApp()->getLevelIndex();
            getApp()->changeState("level" + std::to_string(currentLevelIndex));
        } else if (keyboard.justPressed(GLFW_KEY_L)) {
            collisionSystem.toggleDebugMode();
        } else if (keyboard.justPressed(GLFW_KEY_ESCAPE)) {
            audioSystem.stopSfx("SUPERHOT");
            textRenderer.clearTextQueue();
            fpsController.exit();
            getApp()->changeState("menu");
        }
        if (keyboard.justPressed(GLFW_KEY_F9)) {
            // Toggle the shader debug menu
            Settings& settings = Settings::getInstance();
            settings.showImGuiShaderDebugMenu = !settings.showImGuiShaderDebugMenu;
            // unlock the mouse
            auto& mouse = getApp()->getMouse();
            if (settings.showImGuiShaderDebugMenu) {
                mouse.unlockMouse(getApp()->getWindow());
            } else {
                mouse.lockMouse(getApp()->getWindow());
            }
        }
    }

    void onDestroy() override {
        fpsController.turnOffCrosshair();
        weaponsSystem.onDestroy();
        enemySystem.onDestroy();
        world.clear();
    }
};

// Static member initialization
bool Playstate::initialized = false;
