#pragma once

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

    our::ShaderProgram* program;
    std::unordered_map<std::string,  our::Mesh*> meshes;
    our::Material* material;
    std::vector<our::Transform> transforms;
    glm::mat4 VP;
    std::vector<our::Light> lights;
    our::CameraComponent camera;
    glm::vec3 cameraPosition = glm::vec3(0, 0, 5);
    glm::vec3 cameraTarget = glm::vec3(0, 0, 0);
    glm::vec3 cameraUp = glm::vec3(0, 1, 0);
    float cameraYaw = -90.0f;
    float cameraPitch = 0.0f;
    float cameraZoom = 45.0f;
    float movementSpeed = 5.0f;
    float mouseSensitivity = 0.1f;

    our::Mouse* mouse;
    our::Keyboard* keyboard;

    our::WindowConfiguration getWindowConfiguration() {
        return { "Light", {1280, 720}, false };
    }

    std::unordered_map<std::string, int> lightTypes = {
        {"directional", 0},
        {"point", 1},
        {"spot", 2}
    };

    void updateCamera(double deltaTime) {
        if (!keyboard->isPressed(GLFW_KEY_RIGHT_ALT)) {
            float velocity = movementSpeed * (float)deltaTime;
            glm::vec3 front;
            front.x = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
            front.y = sin(glm::radians(cameraPitch));
            front.z = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
            front = glm::normalize(front);

            glm::vec3 right = glm::normalize(glm::cross(front, cameraUp));

            if (keyboard->isPressed(GLFW_KEY_W)) cameraPosition += velocity * front;
            if (keyboard->isPressed(GLFW_KEY_S)) cameraPosition -= velocity * front;
            if (keyboard->isPressed(GLFW_KEY_A)) cameraPosition -= velocity * right;
            if (keyboard->isPressed(GLFW_KEY_D)) cameraPosition += velocity * right;

            if (mouse->isPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                glm::vec2 delta = mouse->getMouseDelta();
                cameraYaw += delta.x * mouseSensitivity;
                cameraPitch -= delta.y * mouseSensitivity;
                cameraPitch = glm::clamp(cameraPitch, -89.0f, 89.0f);
            }

            cameraZoom -= mouse->getScrollOffset().y;
            cameraZoom = glm::clamp(cameraZoom, 1.0f, 90.0f);
        }

        glm::vec3 front;
        front.x = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
        front.y = sin(glm::radians(cameraPitch));
        front.z = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
        front = glm::normalize(front);

        glm::mat4 view = glm::lookAt(cameraPosition, cameraPosition + front, cameraUp);
        glm::ivec2 size = getApp()->getFrameBufferSize();
        float aspect = float(size.x)/size.y;
        glm::mat4 proj = glm::perspective(glm::radians(cameraZoom), aspect, 0.01f, 1000.0f);

        VP = proj * view;
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
            if(auto& cam = config["camera"]; cam.is_object()){
                cameraPosition = cam.value("eye", glm::vec3(0, 0, 5));
                cameraTarget = cam.value("center", glm::vec3(0, 0, 0));
                cameraUp = cam.value("up", glm::vec3(0, 1, 0));
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
        glClearColor(0.5f, 0.5f, 0.3f, 1.0f);

        std::cout << "LightTestState initialized." << std::endl;
    }

    void onDraw(double deltaTime) override {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        updateCamera(deltaTime);

        if (lights.empty()) {
            std::cerr << "Error: No lights available." << std::endl;
            return;
        }
        if (transforms.empty()) {
            std::cerr << "Error: No transforms available." << std::endl;
            return;
        }

        program->set("camera_position", cameraPosition);
        program->set(("light_count"), static_cast<int>(lights.size()));
        material->setup();

        int light_index = 0;
        for (const auto& light : lights) {
            if (!light.enabled) continue;

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
            program->set("model", transform.toMat4());
            program->set("MVP", VP * transform.toMat4());
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
