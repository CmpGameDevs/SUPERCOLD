#pragma once
#include <application.hpp>
#include <string>
#include <asset-loader.hpp>
#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/collision-system.hpp>
#include <systems/fps-controller.hpp>
#include <systems/movement.hpp>
#include "../utils/time-scale.hpp"


class LevelState : public our::State {
    our::Application* app = nullptr;
    our::World world;
    our::ForwardRenderer renderer;
    our::CollisionSystem collisionSystem;
    our::FPSControllerSystem fpsController;
    our::MovementSystem movementSystem;
    game::TimeScaler timeScaler;
    float timeScale;

    void onInitialize() override {
        app = getApp();
        auto &config = app->getConfig()["scene"];
        if(config.contains("assets")){
            our::deserializeAllAssets(config["assets"]);
        }
        if(config.contains("world")){
            world.deserialize(config["world"]);
        }

        fpsController.enter(app);
        auto size = app->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);
    }

    void onDraw(double deltaTime) override {
        fpsController.update(&world, (float)deltaTime);
        float speed = fpsController.getSpeedMagnitude();
        timeScaler.update(speed);
        timeScale = timeScaler.getTimeScale();
        float scaledDeltaTime = (float)deltaTime * timeScale;
        movementSystem.update(&world, scaledDeltaTime);
        renderer.render(&world);

        auto &keyboard = app->getKeyboard();
        if(keyboard.justPressed(GLFW_KEY_ESCAPE)){
            app->changeState("menu");
        }
    }

    void onDestroy() override {
        renderer.destroy();
        fpsController.exit();
        world.clear();
        our::clearAllAssets();
    }
};