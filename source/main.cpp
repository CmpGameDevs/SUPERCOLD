#include <iostream>
#include <fstream>
#include <flags/flags.h>
#include <json/json.hpp>
#include <filesystem>
#include <application.hpp>
#include <string>
#include "states/menu-state.hpp"
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
#include "states/light-test-state.hpp"
#include "states/physics-test-state.hpp"
namespace fs = std::filesystem;

std::vector<nlohmann::json> parseLevels(int levels_count) {
    std::vector<nlohmann::json> levels;
    for(int i = 0; i < levels_count; i++){
        std::string path =  "config/levels/level" + std::to_string(i + 1) + ".jsonc";
        std::ifstream file_in(path);
        if(!file_in){
            std::cerr << "Couldn't open file: " << path << std::endl;
            return {};
        }
        nlohmann::json level_config = nlohmann::json::parse(file_in, nullptr, true, true);
        file_in.close();
        levels.push_back(level_config);
    }
    return levels;
}

int main(int argc, char** argv) {
    
    flags::args args(argc, argv); // Parse the command line arguments
    // config_path is the path to the json file containing the application configuration
    // Default: "config/app.json"
    std::string config_path = args.get<std::string>("c", "config/app.jsonc");
    // run_for_frames is how many frames to run the application before automatically closing
    // This is useful for testing multiple configurations in a batch
    // Default: 0 where the application runs indefinitely until manually closed
    int run_for_frames = args.get<int>("f", 0);

    // Open the config file and exit if failed
    std::ifstream file_in(config_path);
    if(!file_in){
        std::cerr << "Couldn't open file: " << config_path << std::endl;
        return -1;
    }
    // Read the file into a json object then close the file
    nlohmann::json app_config = nlohmann::json::parse(file_in, nullptr, true, true);

    file_in.close();


    int levels_count = 0;
    try {
        for (const auto& entry : fs::directory_iterator("config/levels")) {
            if (entry.is_regular_file())
                levels_count++;
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error accessing directory: " << e.what() << std::endl;
        return -1;
    }

    std::vector<nlohmann::json> levels_configs = parseLevels(levels_count);

    // Create the application
    our::Application app(app_config, levels_configs);
    
    // Register all the states of the project in the application
    app.registerState<Menustate>("menu");
    for(int i = 0; i < levels_count; i++){
        app.registerState<Playstate>("level" + std::to_string(i + 1));
    }
    // app.registerState<ShaderTestState>("shader-test");
    // app.registerState<MeshTestState>("mesh-test");
    // app.registerState<TransformTestState>("transform-test");
    // app.registerState<PipelineTestState>("pipeline-test");
    // app.registerState<TextureTestState>("texture-test");
    // app.registerState<SamplerTestState>("sampler-test");
    // app.registerState<MaterialTestState>("material-test");
    // app.registerState<EntityTestState>("entity-test");
    // app.registerState<RendererTestState>("renderer-test");
    // app.registerState<LightTestState>("light-test");
    app.registerState<PhysicsTestState>("physics-test");

    // Then choose the state to run based on the option "start-scene" in the config
    if(app_config.contains(std::string{"start-scene"})){
        app.changeState(app_config["start-scene"].get<std::string>());
    }

    // Finally run the application
    // Here, the application loop will run till the termination condition is statisfied
    return app.run(run_for_frames);
}
