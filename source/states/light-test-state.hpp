#pragma once

#include <application.hpp>
#include <utility>
#include <iostream>
#include <mesh/mesh.hpp>
#include <mesh/mesh-utils.hpp>
#include <texture/texture-utils.hpp>
#include <ecs/lighting.hpp>
#include <ecs/lighting.cpp>
#include <ecs/transform.hpp>
#include <components/free-camera-controller.hpp>
#include <components/camera.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <json/json.hpp>
#include <fstream>
#include <unordered_map>
#include <texture/sampler.hpp>
#include <ibl/cubemap-buffer.hpp>
#include <ibl/cubemap.hpp>
#include <texture/cubemap-texture.hpp>
#include <ibl/hdr-system.hpp>

class LightTestState : public our::State {

    our::ShaderProgram* pbr_shader;
    our::Sampler* sampler;
    our::Material* pbr_material;
    our::Transform transform;
    our::HDRSystem* hdrSystem;
    std::unordered_map<std::string,  our::Mesh*> meshes;
    std::vector<our::Light> lights;
    glm::mat4 view;
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 projection;
    glm::vec3 cameraPosition = glm::vec3(0, 0, 5);
    glm::vec3 cameraTarget = glm::vec3(0, 0, 0);
    glm::vec3 cameraUp = glm::vec3(0, 1, 0);
    float near;
    float far;
    float cameraYaw = -90.0f;
    float cameraPitch = 0.0f;
    float cameraZoom = 45.0f;
    float movementSpeed = 5.0f;
    float mouseSensitivity = 0.1f;
    int nrRows;
    int nrColumns;
    float gap;

    our::Mouse* mouse;
    our::Keyboard* keyboard;

    std::unordered_map<std::string, int> lightTypes = {
        {"directional", 0},
        {"point", 1},
        {"spot", 2}
    };

    void updateCamera(double deltaTime) {
        float speedup =  1.0f;
        if (keyboard->isPressed(GLFW_KEY_LEFT_SHIFT)) speedup = 2.0f;
        float velocity = movementSpeed * (float)deltaTime * speedup;
        glm::vec3 front;
        front.x = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
        front.y = sin(glm::radians(cameraPitch));
        front.z = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
        front = glm::normalize(front);

        glm::vec3 right = glm::normalize(glm::cross(front, cameraUp));
        glm::vec3 up = glm::normalize(glm::cross(right, front));

        if (keyboard->isPressed(GLFW_KEY_W)) cameraPosition += velocity * front;
        if (keyboard->isPressed(GLFW_KEY_S)) cameraPosition -= velocity * front;
        if (keyboard->isPressed(GLFW_KEY_A)) cameraPosition -= velocity * right;
        if (keyboard->isPressed(GLFW_KEY_D)) cameraPosition += velocity * right;
        if(keyboard->isPressed(GLFW_KEY_SPACE)) cameraPosition += velocity * up;
        

        if (mouse->isPressed(GLFW_MOUSE_BUTTON_LEFT)) {
            glm::vec2 delta = mouse->getMouseDelta();
            cameraYaw += delta.x * mouseSensitivity;
            cameraPitch -= delta.y * mouseSensitivity;
            cameraPitch = glm::clamp(cameraPitch, -89.0f, 89.0f);
        }

        cameraZoom -= mouse->getScrollOffset().y;
        cameraZoom = glm::clamp(cameraZoom, 1.0f, 90.0f);
        
        view = glm::lookAt(cameraPosition, cameraPosition + front, cameraUp);
        glm::ivec2 size = getApp()->getFrameBufferSize();
        float aspect = float(size.x)/size.y;
        projection = glm::perspective(glm::radians(cameraZoom), aspect, near, far);
    }

    void onInitialize() override {
        auto& config = getApp()->getConfig()["scene"];

        mouse = &getApp()->getMouse();
        keyboard = &getApp()->getKeyboard();
        mouse->setEnabled(true, getApp()->getWindow());
        our::Mouse::lockMouse(getApp()->getWindow());

        if(config.contains("assets")){
            our::deserializeAllAssets(config["assets"]);
        }
        
        
        pbr_shader = our::AssetLoader<our::ShaderProgram>::get("pbr");
        meshes["sphere"] = our::AssetLoader<our::Mesh>::get("mesh");
        pbr_material = our::AssetLoader<our::Material>::get("pbr");
        sampler = our::AssetLoader<our::Sampler>::get("sampler");
        
        
        if(config.contains("grid")){
            if(auto& grid = config["grid"]; grid.is_object()){
                nrRows = grid.value("rows", 1);
                nrColumns = grid.value("columns", 1);
                gap = grid.value("gap", 2.5f);
            }
        }
        
        if(config.contains("camera")){
            if(auto& cam = config["camera"]; cam.is_object()){
                cameraPosition = cam.value("eye", glm::vec3(0, 0, 5));
                cameraTarget = cam.value("center", glm::vec3(0, 0, 0));
                cameraUp = cam.value("up", glm::vec3(0, 1, 0));
                near = cam.value("near", 0.1f);
                far = cam.value("far", 100.0f);
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
        
        pbr_material->setup();
        pbr_shader->use();
        pbr_shader->set("irradianceMap", TEXTURE_UNIT_IRRADIANCE);
        pbr_shader->set("prefilterMap", TEXTURE_UNIT_PREFILTER);
        pbr_shader->set("brdfLUT", TEXTURE_UNIT_BRDF);
        
        hdrSystem = new our::HDRSystem();

        hdrSystem->Initialize();
        std::cout<<"HDRSystem initialized."<<std::endl;
        hdrSystem->setup();
        std::cout<<"HDRSystem setup."<<std::endl;
        
        
        glViewport(0, 0, getApp()->getWindowSize().x, getApp()->getWindowSize().y);

        std::cout << "LightTestState initialized." << std::endl;
    }

    void onDraw(double deltaTime) override {
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        updateCamera(deltaTime);

        pbr_shader->use();
        pbr_shader->set("view", view);
        pbr_shader->set("projection", projection);
        pbr_shader->set("cameraPosition", cameraPosition);
        pbr_shader->set("lightCount", static_cast<int>(lights.size()));
        

        transform.position = glm::vec3(0, 0, 0);
        transform.rotation = glm::vec3(0, 0, 0);
        transform.scale = glm::vec3(1, 1, 1);

        // bind pre-computed IBL data
        hdrSystem->bindTextures();

        for (int row = 0; row < nrRows; ++row) 
        {
            pbr_shader->set("material.metallic", (float)row / (float)nrRows);
            for (int col = 0; col < nrColumns; ++col) 
            {
                // we clamp the roughness to 0.05 - 1.0 as perfectly smooth surfaces (roughness of 0.0) tend to look a bit off
                // on direct lighting.
                pbr_shader->set("material.roughness", glm::clamp((float)col / (float)nrColumns, 0.05f, 1.0f));
                
                model = glm::mat4(1.0f);
                transform.position = glm::vec3(
                    (col - (nrColumns / 2)) * gap, 
                    (row - (nrRows / 2)) * gap, 
                    0.0f
                );
                model = transform.toMat4();
                pbr_shader->set("model", model);
                our::mesh_utils::renderSphere();
            }
        }

        int light_index = 0;
        for (const auto& light : lights) {
            if (!light.enabled) continue;

            std::string prefix = "lights[" + std::to_string(light_index) + "].";

            if(light.realistic){
                pbr_shader->set(prefix + "color", light.color);
            } else {
                pbr_shader->set(prefix + "diffuse", light.diffuse);
                pbr_shader->set(prefix + "specular", light.specular);
                pbr_shader->set(prefix + "ambient", light.ambient);
            }

            switch (light.type) {
                case our::LightType::DIRECTIONAL:
                    pbr_shader->set(prefix + "direction", normalize(light.direction));
                    break;
                case our::LightType::POINT:
                    pbr_shader->set(prefix + "position", light.position);
                    pbr_shader->set(prefix + "attenuation_constant", light.attenuation.constant);
                    pbr_shader->set(prefix + "attenuation_linear", light.attenuation.linear);
                    pbr_shader->set(prefix + "attenuation_quadratic", light.attenuation.quadratic);
                    break;
                case our::LightType::SPOT:
                    pbr_shader->set(prefix + "position", light.position);
                    pbr_shader->set(prefix + "direction", glm::normalize(light.direction));
                    pbr_shader->set(prefix + "attenuation_constant", light.attenuation.constant);
                    pbr_shader->set(prefix + "attenuation_linear", light.attenuation.linear);
                    pbr_shader->set(prefix + "attenuation_quadratic", light.attenuation.quadratic);
                    pbr_shader->set(prefix + "inner_angle", light.spot_angle.inner);
                    pbr_shader->set(prefix + "outer_angle", light.spot_angle.outer);
                    break;
            }
            light_index++;

            model = glm::mat4(1.0f);
            transform.position = light.position;
            transform.scale = glm::vec3(0.5f);
            model = transform.toMat4();
            pbr_shader->set("model", model);
            our::mesh_utils::renderSphere();
        }

        hdrSystem->renderBackground(projection, view);
    }

    void onDestroy() override {
        delete pbr_shader;
        delete pbr_material;
        delete sampler;
        delete hdrSystem;
        delete mouse;
        delete keyboard;
        for(auto& [name, mesh]: meshes){
            delete mesh;
        }
    }
};


