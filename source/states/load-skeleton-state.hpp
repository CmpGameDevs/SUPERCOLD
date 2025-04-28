#pragma once

#include <application.hpp>
#include <asset-loader.hpp>
#include <systems/free-camera-controller.hpp>
#include <systems/forward-renderer.hpp>

class LoadSkeletonState : public our::State {

    our::World world;
    our::ForwardRenderer renderer;
    our::FreeCameraControllerSystem cameraController;

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
    }

    void onDraw(double deltaTime) override {
        cameraController.update(&world, (float)deltaTime);
        renderer.render(&world);
    }

    void onDestroy() override {
        renderer.destroy();
        world.clear();
        our::clearAllAssets();
    }
};


