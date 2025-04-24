#include <iostream>
#include <fstream>
#include <flags/flags.h>
#include <json/json.hpp>

#include <application.hpp>

#include "game/core/menu.hpp"
#include "game/core/level.hpp"
#include "states/play-state.hpp"
#include "states/shader-test-state.hpp"
#include "states/mesh-test-state.hpp"
#include "states/transform-test-state.hpp"
#include "states/pipeline-test-state.hpp"
#include "states/texture-test-state.hpp"
#include "states/sampler-test-state.hpp"
#include "states/material-test-state.hpp"
#include "states/entity-test-state.hpp"
#include "states/renderer-test-state.hpp"
#include "states/physics-test-state.hpp"
#include "states/light-test-state.hpp"


nlohmann::json loadConfig(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Couldn't open config file: " << path << std::endl;
        throw std::runtime_error("Failed to load config file");
    }
    auto config = nlohmann::json::parse(file, nullptr, true, true);
    file.close();
    return config;
}

int main(int argc, char** argv) {
    
    flags::args args(argc, argv); // Parse the command line arguments
    // config_path is the path to the json file containing the application configuration
    // Default: "config/game.json"
    std::string app_path = args.get<std::string>("c", "config/app.jsonc");
    // run_for_frames is how many frames to run the application before automatically closing
    // This is useful for testing multiple configurations in a batch
    // Default: 0 where the application runs indefinitely until manually closed
    int run_for_frames = args.get<int>("f", 0);

    // Read the file into a json object then close the file
    nlohmann::json app_config = loadConfig(app_path);

    // Create the application
    our::Application app(app_config);
    
    // Register all the states of the project in the application
    app.registerState<Menustate>("menu");
    app.registerState<LevelState>("play");
    app.registerState<LevelState>("level2");

    if(app_config.contains(std::string{"start-scene"})){
        app.changeState(app_config["start-scene"].get<std::string>());
    }

    // Finally run the application
    // Here, the application loop will run till the termination condition is statisfied
    return app.run(run_for_frames);
}