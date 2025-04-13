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


const int TEXTURE_UNIT_IRRADIANCE = 9;
const int TEXTURE_UNIT_ENVIRONMENT = 10;
const int TEXTURE_UNIT_HDR = 11;
const int TEXTURE_UNIT_BRDF = 12;
const int TEXTURE_UNIT_PREFILTER = 13;

class LightTestState : public our::State {

    our::ShaderProgram* pbr_shader;
    our::ShaderProgram* equirectangular_shader;
    our::ShaderProgram* irradiance_shader;
    our::ShaderProgram* background_shader;
    our::ShaderProgram* brdf_shader;
    our::ShaderProgram* prefilter_shader;
    our::Sampler* sampler;
    our::Material* pbr_material;
    our::Transform transform;
    our::CubeMapBuffer *cubeMapBuffer;
    our::CubeMapTexture *envCubeMap;
    our::CubeMapTexture *irradianceMap;
    our::CubeMapTexture *prefilterMap;
    our::CubeMap* irradianceCubeMap;
    our::CubeMap* prefilterCubeMap;
    our::CubeMap* equirectangularCubeMap;
    std::unordered_map<std::string,  our::Mesh*> meshes;
    std::vector<our::Light> lights;
    glm::mat4 view;
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 projection;
    glm::vec3 cameraPosition = glm::vec3(0, 0, 5);
    glm::vec3 cameraTarget = glm::vec3(0, 0, 0);
    glm::vec3 cameraUp = glm::vec3(0, 1, 0);
    our::Texture2D* brdfLUTTexture;
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
    bool once = true;

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
        equirectangular_shader = our::AssetLoader<our::ShaderProgram>::get("equirectangular");
        irradiance_shader = our::AssetLoader<our::ShaderProgram>::get("irradiance");
        background_shader = our::AssetLoader<our::ShaderProgram>::get("background");
        brdf_shader = our::AssetLoader<our::ShaderProgram>::get("brdf");
        prefilter_shader = our::AssetLoader<our::ShaderProgram>::get("prefilter");
        meshes["sphere"] = our::AssetLoader<our::Mesh>::get("mesh");
        pbr_material = our::AssetLoader<our::Material>::get("pbr");
        sampler = our::AssetLoader<our::Sampler>::get("sampler");

        cubeMapBuffer = new our::CubeMapBuffer();
        envCubeMap = new our::CubeMapTexture();
        irradianceMap = new our::CubeMapTexture();
        prefilterMap = new our::CubeMapTexture();

        equirectangularCubeMap = new our::EquiRectangularCubeMap(equirectangular_shader, envCubeMap, cubeMapBuffer);
        irradianceCubeMap = new our::IrradianceCubeMap(irradiance_shader, irradianceMap, cubeMapBuffer);
        prefilterCubeMap = new our::PrefilterCubeMap(prefilter_shader, prefilterMap, cubeMapBuffer);
        
        pbr_material->setup();
        pbr_shader->use();
        pbr_shader->set("irradianceMap", TEXTURE_UNIT_IRRADIANCE);
        pbr_shader->set("prefilterMap", TEXTURE_UNIT_PREFILTER);
        pbr_shader->set("brdfLUT", TEXTURE_UNIT_BRDF);
        background_shader->use();
        background_shader->set("environmentMap", TEXTURE_UNIT_ENVIRONMENT);

        
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

        cubeMapBuffer->size = { 512, 512 };
        cubeMapBuffer->setupFrameBuffer();
        cubeMapBuffer->setupRenderBuffer();
        
        our::Texture2D* hdr_texture = our::texture_utils::loadHDR("assets/textures/hdr/circus_backstage.hdr", false);

        envCubeMap->setupCubeTexture(cubeMapBuffer->size);
        
        // pbr: set up projection and view matrices for capturing data onto the 6 cubemap face directions
        // ----------------------------------------------------------------------------------------------
        glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        glm::mat4 captureViews[] =
        {
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
        };


        // pbr: convert HDR equirectangular environment map to cubemap equivalent
        // ----------------------------------------------------------------------
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_HDR);
        hdr_texture->bind();

        equirectangularCubeMap->convertToCubeMap(TEXTURE_UNIT_HDR, captureProjection, captureViews);

        // then let OpenGL generate mipmaps from first mip face (combatting visible dots artifact)
        envCubeMap->generateMipmaps();

        // pbr: create an irradiance cubemap, and re-scale capture FBO to irradiance scale.
        // --------------------------------------------------------------------------------

        cubeMapBuffer->size = { 32, 32 };

        irradianceMap->setupCubeTexture(cubeMapBuffer->size);

        cubeMapBuffer->bind();
        cubeMapBuffer->setRenderBufferStorage(GL_DEPTH_COMPONENT24, cubeMapBuffer->size.x, cubeMapBuffer->size.y);

        // pbr: solve diffuse integral by convolution to create an irradiance (cube)map.
        // -----------------------------------------------------------------------------
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_ENVIRONMENT);
        envCubeMap->bind();

        irradianceCubeMap->convertToCubeMap(TEXTURE_UNIT_ENVIRONMENT, captureProjection, captureViews);


        // pbr: create a pre-filter cubemap, and re-scale capture FBO to pre-filter scale.
        // --------------------------------------------------------------------------------
        cubeMapBuffer->size = { 128, 128 };

        prefilterMap->setupCubeTexture(cubeMapBuffer->size, GL_RGB16F, GL_RGB, GL_FLOAT, true);

        // pbr: run a quasi monte-carlo simulation on the environment lighting to create a prefilter (cube)map.
        // ----------------------------------------------------------------------------------------------------
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_PREFILTER);
        envCubeMap->bind();

        prefilterCubeMap->convertToCubeMap(TEXTURE_UNIT_PREFILTER, captureProjection, captureViews);

        // pbr: generate a 2D LUT from the BRDF equations used.
        // ----------------------------------------------------
        // 1. pre-allocate a texture for the BRDF lookup texture atlas.

        cubeMapBuffer->size = { 512, 512 };

        brdfLUTTexture = new our::Texture2D();
        brdfLUTTexture->bind();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, cubeMapBuffer->size.x, cubeMapBuffer->size.y, 0, GL_RG, GL_FLOAT, 0);
        // be sure to set wrapping mode to GL_CLAMP_TO_EDGE
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // then re-configure capture framebuffer object and render screen-space quad with BRDF shader.

        cubeMapBuffer->bind();
        cubeMapBuffer->setRenderBufferStorage(GL_DEPTH_COMPONENT24, cubeMapBuffer->size.x, cubeMapBuffer->size.y);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture->getOpenGLName(), 0);

        glViewport(0, 0, cubeMapBuffer->size.x, cubeMapBuffer->size.y);

        brdf_shader->use();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        our::mesh_utils::renderQuad();

        cubeMapBuffer->unbindFrameBuffer();
        
        glViewport(0, 0, getApp()->getWindowSize().x, getApp()->getWindowSize().y);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

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
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_IRRADIANCE);
        irradianceMap->bind();
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_PREFILTER);
        prefilterMap->bind();
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_BRDF);
        brdfLUTTexture->bind();
        
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

        background_shader->use();
        background_shader->set("view", view);
        background_shader->set("projection", projection);
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_ENVIRONMENT);
        envCubeMap->bind();
        our::mesh_utils::renderCube();

        once = false;
    }

    void onDestroy() override {
        delete pbr_shader;
        delete pbr_material;
        for(auto& [name, mesh]: meshes){
            delete mesh;
        }
    }
};


