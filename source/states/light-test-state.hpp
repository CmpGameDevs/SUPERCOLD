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


const int TEXTURE_UNIT_IRRADIANCE = 9;
const int TEXTURE_UNIT_ENVIRONMENT = 10;
const int TEXTURE_UNIT_HDR = 11;

// renders (and builds at first invocation) a sphere
// -------------------------------------------------
unsigned int sphereVAO = 0;
unsigned int indexCount;
void renderSphere()
{
    if (sphereVAO == 0)
    {
        glGenVertexArrays(1, &sphereVAO);

        unsigned int vbo, ebo;
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        std::vector<glm::vec3> positions;
        std::vector<glm::vec2> uv;
        std::vector<glm::vec3> normals;
        std::vector<unsigned int> indices;

        const unsigned int X_SEGMENTS = 64;
        const unsigned int Y_SEGMENTS = 64;
        const float PI = 3.14159265359f;
        for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
        {
            for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
            {
                float xSegment = (float)x / (float)X_SEGMENTS;
                float ySegment = (float)y / (float)Y_SEGMENTS;
                float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
                float yPos = std::cos(ySegment * PI);
                float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

                positions.push_back(glm::vec3(xPos, yPos, zPos));
                uv.push_back(glm::vec2(xSegment, ySegment));
                normals.push_back(glm::vec3(xPos, yPos, zPos));
            }
        }

        bool oddRow = false;
        for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
        {
            if (!oddRow) // even rows: y == 0, y == 2; and so on
            {
                for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
                {
                    indices.push_back(y * (X_SEGMENTS + 1) + x);
                    indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                }
            }
            else
            {
                for (int x = X_SEGMENTS; x >= 0; --x)
                {
                    indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                    indices.push_back(y * (X_SEGMENTS + 1) + x);
                }
            }
            oddRow = !oddRow;
        }
        indexCount = static_cast<unsigned int>(indices.size());

        std::vector<float> data;
        for (unsigned int i = 0; i < positions.size(); ++i)
        {
            data.push_back(positions[i].x);
            data.push_back(positions[i].y);
            data.push_back(positions[i].z);
            if (normals.size() > 0)
            {
                data.push_back(normals[i].x);
                data.push_back(normals[i].y);
                data.push_back(normals[i].z);
            }
            if (uv.size() > 0)
            {
                data.push_back(uv[i].x);
                data.push_back(uv[i].y);
            }
        }
        glBindVertexArray(sphereVAO);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
        unsigned int stride = (3 + 2 + 3) * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    }

    glBindVertexArray(sphereVAO);
    glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
}

// renderCube() renders a 1x1 3D cube in NDC.
// -------------------------------------------------
unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube()
{
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
            // front face
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
            // left face
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            // right face
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
            // bottom face
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
            -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
            // top face
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
             1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
            -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}
class LightTestState : public our::State {

    our::ShaderProgram* pbr_shader;
    our::ShaderProgram* equirectangular_shader;
    our::ShaderProgram* irradiance_shader;
    our::ShaderProgram* background_shader;
    our::Sampler* sampler;
    std::unordered_map<std::string,  our::Mesh*> meshes;
    our::Material* pbr_material;
    glm::mat4 model = glm::mat4(1.0f);
    our::Transform transform;
    glm::mat4 view;
    glm::mat4 projection;
    std::vector<our::Light> lights;
    our::CameraComponent camera;
    glm::vec3 cameraPosition = glm::vec3(0, 0, 5);
    glm::vec3 cameraTarget = glm::vec3(0, 0, 0);
    glm::vec3 cameraUp = glm::vec3(0, 1, 0);
    GLuint envCubemap;
    GLuint irradianceMap;
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
        meshes["sphere"] = our::AssetLoader<our::Mesh>::get("mesh");
        pbr_material = our::AssetLoader<our::Material>::get("pbr");
        sampler = our::AssetLoader<our::Sampler>::get("sampler");
        
        pbr_material->setup();
        pbr_shader->use();
        pbr_shader->set("irradianceMap", TEXTURE_UNIT_IRRADIANCE);
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
        
        unsigned int captureFBO = 0, captureRBO = 0;
        our::texture_utils::setupFrameBuffers(captureFBO, captureRBO);
        our::Texture2D* hdr_texture = our::texture_utils::loadHDR("assets/textures/hdr/newport_loft.hdr", false);
        our::texture_utils::setupCubeMapFramebuffer(envCubemap, 512);

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
        equirectangular_shader->use();
        equirectangular_shader->set("equirectangularMap", TEXTURE_UNIT_HDR);
        equirectangular_shader->set("projection", captureProjection);
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_HDR);
        hdr_texture->bind();

        glViewport(0, 0, 512, 512); // don't forget to configure the viewport to the capture dimensions.
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        for (unsigned int i = 0; i < 6; ++i)
        {
            equirectangular_shader->set("view", captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            renderCube();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);


        // pbr: create an irradiance cubemap, and re-scale capture FBO to irradiance scale.
        // --------------------------------------------------------------------------------
        our::texture_utils::setupCubeMapFramebuffer(irradianceMap, 32);

        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);

        // pbr: solve diffuse integral by convolution to create an irradiance (cube)map.
        // -----------------------------------------------------------------------------
        irradiance_shader->use();
        irradiance_shader->set("environmentMap", TEXTURE_UNIT_ENVIRONMENT);
        irradiance_shader->set("projection", captureProjection);
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_ENVIRONMENT);
        glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

        glViewport(0, 0, 32, 32); // don't forget to configure the viewport to the capture dimensions.
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        for (unsigned int i = 0; i < 6; ++i)
        {
            irradiance_shader->set("view", captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            renderCube();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        std::cout << "Loaded HDR texture: "  << std::endl;
        
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
        glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
        
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
                renderSphere();
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
            renderSphere();
        }

        background_shader->use();
        background_shader->set("view", view);
        background_shader->set("projection", projection);
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_ENVIRONMENT);
        glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
        // glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
        renderCube();

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


