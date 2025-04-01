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
    std::unordered_map<our::LightType, our::ShaderProgram*> programs;
    
    std::unordered_map<std::string,  our::Mesh*> meshes;
    
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
        
        programs[our::LightType::DIRECTIONAL] = new our::ShaderProgram();
        programs[our::LightType::DIRECTIONAL]->attach("assets/shaders/light/light_transform.vert", GL_VERTEX_SHADER);
        programs[our::LightType::DIRECTIONAL]->attach("assets/shaders/light/directional_light.frag", GL_FRAGMENT_SHADER);
        programs[our::LightType::DIRECTIONAL]->link();

        programs[our::LightType::POINT] = new our::ShaderProgram();
        programs[our::LightType::POINT]->attach("assets/shaders/light/light_transform.vert", GL_VERTEX_SHADER);
        programs[our::LightType::POINT]->attach("assets/shaders/light/point_light.frag", GL_FRAGMENT_SHADER);
        programs[our::LightType::POINT]->link();

        programs[our::LightType::SPOT] = new our::ShaderProgram();
        programs[our::LightType::SPOT]->attach("assets/shaders/light/light_transform.vert", GL_VERTEX_SHADER);
        programs[our::LightType::SPOT]->attach("assets/shaders/light/spot_light.frag", GL_FRAGMENT_SHADER);
        programs[our::LightType::SPOT]->link();

        std::string meshPath = config["assets"]["meshes"].value("mesh", "");
        if(meshPath.size() == 0){
            std::cerr << "No mesh path provided in the config file." << std::endl;
            return;
        }
        std::cout << "Loading mesh: " << meshPath << std::endl;
        meshes["monkey"] = our::mesh_utils::loadOBJ(meshPath);


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

        // We will use blending to combine the results of the different shaders together.
        // The combination will be additive so we will GL_FUNC_ADD as our blend equation.
        glBlendEquation(GL_FUNC_ADD);
        // We are not going to put alpha in our considerations for simplicity.
        // So to do basic addition, we will just multiply both the source and the destination by 1.
        glBlendFunc(GL_ONE, GL_ONE);
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
    
        bool first_light = true;
        for (auto& light : lights) {
            if (!light.enabled) continue;
    
            auto program = programs[light.type];
            if (!program) {
                std::cerr << "Error: Shader program is null for light type." << std::endl;
                continue;
            }
            program->use();
    
            if (first_light) {
                glDisable(GL_BLEND);
                first_light = false;
            } else {
                glEnable(GL_BLEND);
            }
    
            program->set("light.diffuse", light.diffuse);
            program->set("light.specular", light.specular);
            program->set("light.ambient", light.ambient);
    
            switch (light.type) {
                case our::LightType::DIRECTIONAL:
                    program->set("light.direction", normalize(light.direction));
                    break;
                case our::LightType::POINT:
                    program->set("light.position", light.position);
                    program->set("light.attenuation_constant", light.attenuation.constant);
                    program->set("light.attenuation_linear", light.attenuation.linear);
                    program->set("light.attenuation_quadratic", light.attenuation.quadratic);
                    break;
                case our::LightType::SPOT:
                    program->set("light.position", light.position);
                    program->set("light.direction", glm::normalize(light.direction));
                    program->set("light.attenuation_constant", light.attenuation.constant);
                    program->set("light.attenuation_linear", light.attenuation.linear);
                    program->set("light.attenuation_quadratic", light.attenuation.quadratic);
                    program->set("light.inner_angle", light.spot_angle.inner);
                    program->set("light.outer_angle", light.spot_angle.outer);
                    break;
            }

            program->set("viewPos", glm::vec3(0.0f, 0.0f, 0.0f));

            program->set("materialShininess", 10.0f);
            program->set("materialDiffuse", glm::vec3(0.2f, 0.2f, 0.2f));
            program->set("materialSpecular", glm::vec3(0.2f, 0.2f, 0.2f));
            program->set("materialAmbient", glm::vec3(0.2f, 0.2f, 0.2f));
    
            for (auto& transform : transforms) {
                program->set("transform", VP * transform.toMat4());
                meshes["monkey"]->draw();
            }
        }
    }

    void onDestroy() override {
        for(auto& [type, program]: programs){
                delete program;
        }
        for(auto& [name, mesh]: meshes){
            delete mesh;
        }
    }

};
