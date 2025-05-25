#include <json/json.hpp>
#include <application.hpp>
#include <filesystem>
#include <flags/flags.h>
#include <fstream>
#include <iostream>
#include <string>
#include "states/entity-test-state.hpp"
#include "states/light-test-state.hpp"
#include "states/material-test-state.hpp"
#include "states/menu-state.hpp"
#include "states/mesh-test-state.hpp"
#include "states/physics-test-state.hpp"
#include "states/pipeline-test-state.hpp"
#include "states/play-state.hpp"
#include "states/renderer-test-state.hpp"
#include "states/sampler-test-state.hpp"
#include "states/shader-test-state.hpp"
#include "states/texture-test-state.hpp"
#include "states/transform-test-state.hpp"

// --- Assimp Sanity Check ---
#include <assimp/Importer.hpp>  // For Assimp::Importer
#include <assimp/postprocess.h> // For aiProcess_xxx flags
#include <assimp/scene.h>       // For aiScene
#include <assimp/version.h>     // For aiGetVersionMajor/Minor/Revision

void performAssimpSanityCheck() {
    std::cout << "--- Performing Assimp Sanity Check ---" << std::endl;

    // 1. Attempt to create an Assimp Importer instance
    std::cout << "Attempting to create Assimp Importer..." << std::endl;
    try {
        Assimp::Importer importer;

        // If the above line compiled, let's try a trivial operation.
        // This also helps confirm basic linking.
        const aiScene* scene = importer.ReadFile("dummy_non_existent_file.obj", aiProcess_Triangulate);
        if (scene == nullptr) {
            std::cout << "  [SUCCESS] Assimp Importer created and ReadFile called (expectedly failed for a dummy file)."
                      << std::endl;
            std::cout << "  Importer error string: " << importer.GetErrorString() << std::endl;
        } else {
            // This should not happen with a non-existent file
            std::cerr << "  [WARNING] Assimp Importer ReadFile unexpectedly succeeded for a dummy file." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "  [ERROR] Exception while creating or using Assimp Importer: " << e.what() << std::endl;
        std::cerr << "  This could indicate a problem with linking or runtime dependencies." << std::endl;
    } catch (...) {
        std::cerr << "  [ERROR] Unknown exception while creating or using Assimp Importer." << std::endl;
    }

    // 2. Check and print the linked Assimp version
    unsigned int major = aiGetVersionMajor();
    unsigned int minor = aiGetVersionMinor();
    unsigned int revision = aiGetVersionRevision(); // Also known as patch

    std::cout << "Linked Assimp library version: " << major << "." << minor << "." << revision << std::endl;

    // Compare with the version vcpkg reported installing (5.4.3)
    unsigned int expectedVcpkgMajor = 5;
    unsigned int expectedVcpkgMinor = 4;

    if (major == expectedVcpkgMajor && minor == expectedVcpkgMinor) {
        std::cout << "  [INFO] Linked Assimp version major.minor (" << major << "." << minor
                  << ") matches expected vcpkg version (" << expectedVcpkgMajor << "." << expectedVcpkgMinor << ".x)."
                  << std::endl;
    } else {
        std::cout << "  [WARNING] Linked Assimp version (" << major << "." << minor << "." << revision
                  << ") does NOT match expected vcpkg major.minor (" << expectedVcpkgMajor << "." << expectedVcpkgMinor
                  << ".x)." << std::endl;
        std::cout << "  Please ensure your vcpkg Assimp installation (expected " << expectedVcpkgMajor << "."
                  << expectedVcpkgMinor << ".3) is correctly found by CMake." << std::endl;
    }

    std::cout << "--- Assimp Sanity Check Complete ---" << std::endl << std::endl;
}

namespace fs = std::filesystem;

std::vector<nlohmann::json> parseLevels(int levels_count) {
    std::vector<nlohmann::json> levels;
    for (int i = 0; i < levels_count; i++) {
        std::string path = "config/levels/level" + std::to_string(i + 1) + ".jsonc";
        std::ifstream file_in(path);
        if (!file_in) {
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

    performAssimpSanityCheck();

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
    if (!file_in) {
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
    for (int i = 0; i < levels_count; i++) {
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
    if (app_config.contains(std::string{"start-scene"})) {
        app.changeState(app_config["start-scene"].get<std::string>());
    }

    // Finally run the application
    // Here, the application loop will run till the termination condition is statisfied
    return app.run(run_for_frames);
}
