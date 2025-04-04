#include <application.hpp>
#include <utility>
#include <iostream>
#include <mesh/mesh.hpp>
#include <mesh/mesh-utils.hpp>
#include <ecs/lighting.hpp>
#include <ecs/lighting.cpp>
#include <ecs/transform.hpp>
#include <components/free-camera-controller.hpp>
#include <components/camera.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <json/json.hpp>

#include <fstream>
#include <unordered_map>


class LightTestState : public our::State {

    // We will create a different shader program for each light type.
    our::ShaderProgram* program;
    
    std::unordered_map<std::string,  our::Mesh*> meshes;
    our::Material* material;
    
    std::vector<our::Transform> transforms;
    glm::mat4 VP;
    
    std::vector<our::Light> lights;
    
    our::WindowConfiguration getWindowConfiguration() {
        return { "Light", {1280, 720}, false };
    }
    std::unordered_map<std::string, int> lightTypes = {
        {"directional", 0},
        {"point", 1},
        {"spot", 2}
    };
    
    void onInitialize() override {
        // We will create a different shader program for each light type.
        auto& config = getApp()->getConfig()["scene"];

        // If we have assets in the scene config, we deserialize them
        if(config.contains("assets")){
            our::deserializeAllAssets(config["assets"]);
        }

        program = our::AssetLoader<our::ShaderProgram>::get("lit");
        meshes["sphere"] = our::AssetLoader<our::Mesh>::get("mesh");
        material = our::AssetLoader<our::Material>::get("material");
        program->use();

        transforms.clear();
        if(config.contains("objects")){
            if(auto& objects = config["objects"]; objects.is_array()){
                for(auto& object : objects){
                    our::Transform transform;
                    transform.deserialize(object);
                    transforms.push_back(transform);
                }
            }
        }
        if(config.contains("camera")){
            if(auto& camera = config["camera"]; camera.is_object()){
                glm::vec3 eye = camera.value("eye", glm::vec3(0, 0, 0));
                glm::vec3 center = camera.value("center", glm::vec3(0, 0, -1));
                glm::vec3 up = camera.value("up", glm::vec3(0, 1, 0));
                glm::mat4 V = glm::lookAt(eye, center, up);

                float fov = glm::radians(camera.value("fov", 90.0f));
                float near = camera.value("near", 0.01f);
                float far = camera.value("far", 1000.0f);

                glm::ivec2 size = getApp()->getFrameBufferSize();
                float aspect = float(size.x)/size.y;
                glm::mat4 P = glm::perspective(fov, aspect, near, far);

                VP = P * V;
            }
        }
        
        if(config.contains("lights")){
            if(auto& lightsConfig = config["lights"]; lightsConfig.is_array()){
                for(auto& lightConfig : lightsConfig){
                    our::Light light;
                    if(lightTypes.find(lightConfig.value("type", "")) != lightTypes.end()){
                        light.deserialize(lightConfig);
                        lights.push_back(light);
                    }else{
                        std::cerr << "Unknown light type: " << lightConfig.value("type", "") << std::endl;
                    }
                }
            }
        }


        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        // // We will use blending to combine the results of the different shaders together.
        // // The combination will be additive so we will GL_FUNC_ADD as our blend equation.
        // glBlendEquation(GL_FUNC_ADD);
        // // We are not going to put alpha in our considerations for simplicity.
        // // So to do basic addition, we will just multiply both the source and the destination by 1.
        // glBlendFunc(GL_ONE, GL_ONE);
        std::cout << "LightTestState initialized." << std::endl;
    }

    void onDraw(double deltaTime) override {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
        if (lights.empty()) {
            std::cerr << "Error: No lights available." << std::endl;
            return;
        }
    
        if (transforms.empty()) {
            std::cerr << "Error: No transforms available." << std::endl;
            return;
        }

        // program->set("viewPos", glm::vec3(0.0f, 0.0f, 0.0f));
        program->set("camera_position", glm::vec3(0.0f, 0.0f, 0.0f));
        material->setup();   
        
        bool first_light = true;
        int light_index = 0;
        const int MAX_LIGHT_COUNT = 16;
        for (const auto& light : lights) {
            if (!light.enabled) continue;
    
            // if (first_light) {
            //     glDisable(GL_BLEND);
            //     first_light = false;
            // } else {
            //     glEnable(GL_BLEND);
            // }

            std::string prefix = "lights[" + std::to_string(light_index) + "].";
            
            if(light.realistic){
                program->set(prefix + "color", light.color);
            } else {
                program->set(prefix + "diffuse", light.diffuse);
                program->set(prefix + "specular", light.specular);
                program->set(prefix + "ambient", light.ambient);
            }
            
    
            switch (light.type) {
                case our::LightType::DIRECTIONAL:
                    program->set(prefix + "direction", normalize(light.direction));
                    break;
                case our::LightType::POINT:
                    program->set(prefix + "position", light.position);
                    program->set(prefix + "attenuation_constant", light.attenuation.constant);
                    program->set(prefix + "attenuation_linear", light.attenuation.linear);
                    program->set(prefix + "attenuation_quadratic", light.attenuation.quadratic);
                    break;
                case our::LightType::SPOT:
                    program->set(prefix + "position", light.position);
                    program->set(prefix + "direction", glm::normalize(light.direction));
                    program->set(prefix + "attenuation_constant", light.attenuation.constant);
                    program->set(prefix + "attenuation_linear", light.attenuation.linear);
                    program->set(prefix + "attenuation_quadratic", light.attenuation.quadratic);
                    program->set(prefix + "inner_angle", light.spot_angle.inner);
                    program->set(prefix + "outer_angle", light.spot_angle.outer);
                    break;
            }
            light_index++;
        }

        for (auto& transform : transforms) {
            program->set("transform", VP * transform.toMat4());
            meshes["sphere"]->draw();
        }
    }

    void onDestroy() override {
        delete program;
        delete material;
        for(auto& [name, mesh]: meshes){
            delete mesh;
        }
    }

};
