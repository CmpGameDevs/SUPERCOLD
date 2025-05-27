#include "forward-renderer.hpp"
#include "../mesh/mesh-utils.hpp"
#include "../texture/texture-utils.hpp"
#include <systems/trail-system.hpp>

namespace our {

void ForwardRenderer::initialize(glm::ivec2 windowSize, const nlohmann::json &config) {
    // First, we store the window size for later use
    this->windowSize = windowSize;

    if (config.contains("hdr")) {
        // Create the HDR system and deserialize it using the configuration
        this->hdrSystem = new HDRSystem();
        this->hdrSystem->deserialize(config["hdr"]);
        this->hdrSystem->initialize();
        this->hdrSystem->setup(windowSize);
    }

    // Then we check if there is a sky texture in the configuration
    if (config.contains("sky")) {
        // First, we create a sphere which will be used to draw the sky
        this->skySphere = mesh_utils::sphere(glm::ivec2(16, 16));

        // We can draw the sky using the same shader used to draw textured objects
        ShaderProgram *skyShader = new ShaderProgram();
        skyShader->attach("assets/shaders/textured.vert", GL_VERTEX_SHADER);
        skyShader->attach("assets/shaders/textured.frag", GL_FRAGMENT_SHADER);
        skyShader->link();

        // TODO: (Req 10) Pick the correct pipeline state to draw the sky
        //  Hints: the sky will be draw after the opaque objects so we would need depth testing but which depth funtion
        //  should we pick? We will draw the sphere from the inside, so what options should we pick for the face
        //  culling.
        PipelineState skyPipelineState{};
        skyPipelineState.depthTesting.enabled = true;
        skyPipelineState.depthTesting.function = GL_LEQUAL;

        skyPipelineState.faceCulling.enabled = true;
        skyPipelineState.faceCulling.culledFace = GL_FRONT;

        skyPipelineState.setup();

        // Load the sky texture (note that we don't need mipmaps since we want to avoid any unnecessary blurring while
        // rendering the sky)
        std::string skyTextureFile = config.value<std::string>("sky", "");
        Texture2D *skyTexture = texture_utils::loadImage(skyTextureFile, false);

        // Setup a sampler for the sky
        Sampler *skySampler = new Sampler();
        skySampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        skySampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        skySampler->set(GL_TEXTURE_WRAP_S, GL_REPEAT);
        skySampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Combine all the aforementioned objects (except the mesh) into a material
        this->skyMaterial = new TexturedMaterial();
        this->skyMaterial->shader = skyShader;
        this->skyMaterial->texture = skyTexture;
        this->skyMaterial->sampler = skySampler;
        this->skyMaterial->pipelineState = skyPipelineState;
        this->skyMaterial->tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        this->skyMaterial->alphaThreshold = 1.0f;
        this->skyMaterial->transparent = false;
    }

    // Then we check if there is a postprocessing shader in the configuration
    if (config.contains("postprocess")) {
        postprocess = new PostProcess();
        postprocess->init(windowSize, config["postprocess"]);
    }
        if(config.contains("crosshair"))
        {
            crosshair = Crosshair::getInstance();
            crosshair->initialize(config["crosshair"]);
        }
}

void ForwardRenderer::destroy() {
    // Delete all objects related to the sky
    if (skyMaterial) {
        delete skySphere;
        delete skyMaterial->shader;
        delete skyMaterial->texture;
        delete skyMaterial->sampler;
        delete skyMaterial;
    }
    // Delete all objects related to post processing
    if (postprocess) {
        postprocess->destroy();
        delete postprocess;
    }

    if (hdrSystem) {
        delete hdrSystem;
    }
}

void ForwardRenderer::render(World *world) {
    // First of all, we search for a camera and for all the mesh renderers
    CameraComponent *camera = nullptr;
    opaqueCommands.clear();
    transparentCommands.clear();
    modelCommands.clear();
    for (auto entity : world->getEntities()) {
        // If we hadn't found a camera yet, we look for a camera in this entity
        if (!camera)
            camera = entity->getComponent<CameraComponent>();
        // If this entity has a mesh renderer component
        if (auto meshRenderer = entity->getComponent<MeshRendererComponent>(); meshRenderer) {
            // We construct a command from it
            RenderCommand command;
            command.localToWorld = meshRenderer->getOwner()->getLocalToWorldMatrix();
            command.center = glm::vec3(command.localToWorld * glm::vec4(0, 0, 0, 1));
            command.mesh = meshRenderer->mesh;
            command.material = meshRenderer->material;
            // if it is transparent, we add it to the transparent commands list
            if (command.material->transparent) {
                transparentCommands.push_back(command);
            } else {
                // Otherwise, we add it to the opaque command list
                opaqueCommands.push_back(command);
            }
        }

        if (auto model_renderer = entity->getComponent<ModelComponent>(); model_renderer) {
            // We construct a command from it
            RenderCommand command;
            command.localToWorld = model_renderer->getOwner()->getLocalToWorldMatrix();
            command.center = glm::vec3(command.localToWorld * glm::vec4(0, 0, 0, 1));
            command.model = model_renderer->model;
            modelCommands.push_back(command);
        }
    }

    // If there is no camera, we return (we cannot render without a camera)
    if (camera == nullptr)
        return;

    // TODO: (Req 9) Modify the following line such that "cameraForward" contains a vector pointing the camera forward
    // direction
    //  HINT: See how you wrote the CameraComponent::getViewMatrix, it should help you solve this one
    auto cameraMatrix = camera->getOwner()->getLocalToWorldMatrix();
    glm::vec3 cameraForward = glm::normalize(glm::vec3(cameraMatrix[2]));
    std::sort(transparentCommands.begin(), transparentCommands.end(),
              [cameraForward](const RenderCommand &first, const RenderCommand &second) {
                  // TODO: (Req 9) Finish this function
                  //  HINT: the following return should return true "first" should be drawn before "second".
                  float distance1 = glm::dot(first.center, cameraForward);
                  float distance2 = glm::dot(second.center, cameraForward);
                  return distance1 < distance2;
              });

    // TODO: (Req 9) Get the camera ViewProjection matrix and store it in VP
    glm::mat4 view = camera->getViewMatrix();
    glm::mat4 projection = camera->getProjectionMatrix(windowSize);
    glm::mat4 VP = projection * view;
    // TODO: (Req 9) Set the OpenGL viewport using viewportStart and viewportSize
    glm::vec2 viewportStart = glm::vec2(0, 0);
    glm::vec2 viewportSize = windowSize;

    // Set the OpenGL viewport
    glViewport(viewportStart.x, viewportStart.y, viewportSize.x, viewportSize.y);

    // TODO: (Req 9) Set the clear color to black and the clear depth to 1
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0f);

    // TODO: (Req 9) Set the color mask to true and the depth mask to true (to ensure the glClear will affect the
    // framebuffer)
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    float bloomBrightnessCutoff = 1.0f;
    // If there is a postprocess material, bind the framebuffer
    if (postprocess) {
        // Set current VP matrices for motion blur
        postprocess->setViewProjectionMatrix(VP);
        // TODO: (Req 11) bind the framebuffer
        postprocess->bind();
        bloomBrightnessCutoff = postprocess->getBloomBrightnessCutoff();
    }

    // TODO: (Req 9) Clear the color and depth buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //! The order of the hdrSystem is important
    if (this->hdrSystem) {
        // bind pre-computed IBL data
        this->hdrSystem->bindTextures();
    }

    // TODO: (Req 9) Draw all the opaque commands
    //  Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
    for (auto &command : opaqueCommands) {
        glm::mat4 MVP = VP * command.localToWorld;
        command.material->setup();
        command.material->shader->set("transform", MVP);
        command.material->shader->set("cameraPosition", camera->getOwner()->localTransform.position);
        command.material->shader->set("view", view);
        command.material->shader->set("projection", projection);
        command.material->shader->set("model", command.localToWorld);
        command.material->shader->set("bloomBrightnessCutoff", bloomBrightnessCutoff);
        command.mesh->draw();
    }

    // If there is a sky material, draw the sky
    if (this->skyMaterial) {
        // TODO: (Req 10) setup the sky material
        this->skyMaterial->setup();

        // TODO: (Req 10) Get the camera position
        glm::vec3 cameraPosition = camera->getOwner()->localTransform.position;

        // TODO: (Req 10) Create a model matrix for the sky such that it always follows the camera (sky sphere center =
        // camera position)
        our::Transform skyTransformer;
        skyTransformer.position = cameraPosition;
        glm::mat4 skyModel = skyTransformer.toMat4();

        // TODO: (Req 10) We want the sky to be drawn behind everything (in NDC space, z=1)
        //  We can acheive the is by multiplying by an extra matrix after the projection but what values should we put
        //  in it?
        glm::mat4 alwaysBehindTransform =
            glm::mat4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);

        // TODO: (Req 10) set the "transform" uniform
        glm::mat4 finalTransform = alwaysBehindTransform * VP * skyModel;
        this->skyMaterial->shader->set("transform", finalTransform);

        // TODO: (Req 10) draw the sky sphere
        this->skySphere->draw();
    }

    for (auto &command : modelCommands) {
        command.model->draw(camera, command.localToWorld, windowSize, bloomBrightnessCutoff);
    }

    //! The order of the hdrSystem is important
    if (this->hdrSystem) {
        // Render the background if the HDR system is enabled
        hdrSystem->renderBackground(projection, view, 10);
    }
    
    // Trails act similar to transparent objects
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    
    glm::vec3 cameraRight = glm::vec3(glm::inverse(view)[0]);
    TrailSystem &trailSystem = TrailSystem::getInstance();
    trailSystem.renderTrails(world, view, projection, cameraRight, bloomBrightnessCutoff);

    glDepthMask(GL_TRUE); // Re-enable writing to depth buffer
    glDisable(GL_BLEND);

    // TODO: (Req 9) Draw all the transparent commands
    //  Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
    for (auto &command : transparentCommands) {
        glm::mat4 MVP = VP * command.localToWorld;
        command.material->setup();
        command.material->shader->set("transform", MVP);
        command.material->shader->set("cameraPosition", camera->getOwner()->localTransform.position);
        command.material->shader->set("view", view);
        command.material->shader->set("projection", projection);
        command.material->shader->set("model", command.localToWorld);
        command.material->shader->set("bloomBrightnessCutoff", bloomBrightnessCutoff);
        command.mesh->draw();
    }

    // If there is a postprocess material, apply postprocessing
    if (postprocess) {
        postprocess->renderPostProcess();
        // Update previous VP matrix for next frame's motion blur
        postprocess->updatePreviousViewProjectionMatrix();
    }

    if(crosshair) {
        crosshair->render();
    }

    // If there is a camera, calculate motion direction for motion blur
    if (camera && postprocess && postprocess->isMotionBlurEnabled()) {
        // Get the camera's forward direction
        glm::vec3 cameraForward = glm::normalize(glm::vec3(camera->getOwner()->getLocalToWorldMatrix()[2]));

        // Project camera's forward vector to screen space
        glm::vec2 motionDir = glm::normalize(glm::vec2(cameraForward.x, cameraForward.y));

        // Set motion direction in post-process system
        postprocess->setMotionDirection(motionDir);
    }
}

} // namespace our