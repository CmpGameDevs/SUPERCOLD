#include "forward-renderer.hpp"
#include "../mesh/mesh-utils.hpp"
#include "../texture/texture-utils.hpp"

namespace our
{

    void ForwardRenderer::initialize(glm::ivec2 windowSize, const nlohmann::json &config)
    {
        // First, we store the window size for later use
        this->windowSize = windowSize;

        // Check if the configuration contains a HDR system
        this->hdrSystem = new HDRSystem();

        if (config.contains("hdr"))
        {
            // Create the HDR system and deserialize it using the configuration
            this->hdrSystem->deserialize(config["hdr"]);
            this->hdrSystem->initialize();
            this->hdrSystem->setup(windowSize);
        }
        else
        {
            // If there is no HDR system, we set it to nullptr
            hdrSystem->enable = false;
        }

        // Then we check if there is a sky texture in the configuration
        if (config.contains("sky"))
        {
            // First, we create a sphere which will be used to draw the sky
            this->skySphere = mesh_utils::sphere(glm::ivec2(16, 16));

            // We can draw the sky using the same shader used to draw textured objects
            ShaderProgram *skyShader = new ShaderProgram();
            skyShader->attach("assets/shaders/textured.vert", GL_VERTEX_SHADER);
            skyShader->attach("assets/shaders/textured.frag", GL_FRAGMENT_SHADER);
            skyShader->link();

            // TODO: (Req 10) Pick the correct pipeline state to draw the sky
            //  Hints: the sky will be draw after the opaque objects so we would need depth testing but which depth funtion should we pick?
            //  We will draw the sphere from the inside, so what options should we pick for the face culling.
            PipelineState skyPipelineState{};
            skyPipelineState.depthTesting.enabled = true;
            skyPipelineState.depthTesting.function = GL_LEQUAL;

            skyPipelineState.faceCulling.enabled = true;
            skyPipelineState.faceCulling.culledFace = GL_FRONT;

            skyPipelineState.setup();

            // Load the sky texture (note that we don't need mipmaps since we want to avoid any unnecessary blurring while rendering the sky)
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
        if (config.contains("postprocess"))
        {
            nlohmann::json data = config["postprocess"];
            // Read postprocess parameters
            bloomEnabled = data.value("bloomEnabled", false);
            bloomIntensity = data.value("bloomIntensity", 1.0f);
            bloomIterations = data.value("bloomIterations", 10);
            bloomDirection = data.value("bloomDirection", BloomDirection::BOTH);
            tonemappingEnabled = data.value("tonemappingEnabled", false);
            gammaCorrectionFactor = data.value("gammaCorrectionFactor", 2.2f);
            bloomBrightnessCutoff = data.value("bloomBrightnessCutoff", 1.0f);

            // Create the bloom framebuffer
            for (int i = 0; i < 2; i++)
            {
                bloomBuffers[i] = new BloomFramebuffer(windowSize.x, windowSize.y);
                bloomBuffers[i]->init();
            }

            // Create the bloom shader
            bloomShader = our::AssetLoader<our::ShaderProgram>::get("bloom");

            // TODO: (Req 11) Create a framebuffer
            glGenFramebuffers(1, &postprocessFrameBuffer);

            // TODO: (Req 11) Create a color and a depth texture and attach them to the framebuffer
            //  Hints: The color format can be (Red, Green, Blue and Alpha components with 8 bits for each channel).
            //  The depth format can be (Depth component with 24 bits).
            colorTarget = new Texture2D();
            colorTarget->bind();

            GLsizei levelsCnt = (GLsizei)glm::floor(glm::log2((float)glm::max(windowSize.x, windowSize.y))) + 1;
            glTexStorage2D(GL_TEXTURE_2D, levelsCnt, GL_RGBA8, windowSize.x, windowSize.y);

            depthTarget = new Texture2D();
            depthTarget->bind();

            glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT24, windowSize.x, windowSize.y);

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postprocessFrameBuffer);

            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTarget->getOpenGLName(), 0);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTarget->getOpenGLName(), 0);

            // create bloom texture
            glGenTextures(1, &bloomColorTexture);
            glBindTexture(GL_TEXTURE_2D, bloomColorTexture);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, windowSize.x, windowSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, bloomColorTexture, 0);

            glBindTexture(GL_TEXTURE_2D, 0);

            unsigned int colorAttachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
            glDrawBuffers(2, colorAttachments);

            // TODO: (Req 11) Unbind the framebuffer just to be safe
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

            // Create a vertex array to use for drawing the texture
            glGenVertexArrays(1, &postProcessVertexArray);

            // Create a sampler to use for sampling the scene texture in the post processing shader
            Sampler *postprocessSampler = new Sampler();
            postprocessSampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            postprocessSampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            postprocessSampler->set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            postprocessSampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Create the post processing shader
            ShaderProgram *postprocessShader = new ShaderProgram();
            postprocessShader->attach("assets/shaders/postprocess/post.vert", GL_VERTEX_SHADER);
            postprocessShader->attach(data.value<std::string>("fs", ""), GL_FRAGMENT_SHADER);
            postprocessShader->link();

            // Create a post processing material
            postprocessMaterial = new TexturedMaterial();
            postprocessMaterial->shader = postprocessShader;
            postprocessMaterial->texture = colorTarget;
            postprocessMaterial->sampler = postprocessSampler;
            // The default options are fine but we don't need to interact with the depth buffer
            // so it is more performant to disable the depth mask
            postprocessMaterial->pipelineState.depthMask = false;
        }

        fullscreenQuad = new FullscreenQuad();
    }

    void ForwardRenderer::renderBloom()
    {
        if (!bloomEnabled)
            return; // Skip if bloom is disabled

        glViewport(0, 0, windowSize.x, windowSize.y);

        // Define blur directions
        glm::vec2 blurDirectionX = glm::vec2(1.0f, 0.0f);
        glm::vec2 blurDirectionY = glm::vec2(0.0f, 1.0f);

        // No need to modify directions for BOTH mode - we'll use both
        if (bloomDirection == BloomDirection::HORIZONTAL)
        {
            blurDirectionY = blurDirectionX;
        }
        else if (bloomDirection == BloomDirection::VERTICAL)
        {
            blurDirectionX = blurDirectionY;
        }

        // Rest of your bloom setup
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, bloomColorTexture);
        glGenerateMipmap(GL_TEXTURE_2D);

        bloomShader->use();
        bloomShader->set("inputColorTexture", 0);

        for (auto mipLevel = 0; mipLevel <= 2; mipLevel++)
        {
            bloomBuffers[0]->setMipLevel(mipLevel);
            bloomBuffers[1]->setMipLevel(mipLevel);

            // First pass (horizontal)
            bloomBuffers[0]->bind();
            glBindTexture(GL_TEXTURE_2D, bloomColorTexture);
            bloomShader->set("sampleMipLevel", mipLevel);
            bloomShader->set("blurDirection", blurDirectionX); // Always use X first
            fullscreenQuad->Draw();

            // Second pass (vertical) - applied to the result of horizontal
            bloomBuffers[1]->bind();
            glBindTexture(GL_TEXTURE_2D, bloomBuffers[0]->getColorTextureId());
            bloomShader->set("blurDirection", blurDirectionY); // Then use Y
            fullscreenQuad->Draw();

            // Now bloomBuffers[1] contains a proper circular blur

            // Additional iterations (apply more blur passes if needed)
            unsigned int bloomFramebuffer = 0;

            for (auto i = 1; i < bloomIterations; i++)
            {
                // Horizontal pass
                bloomBuffers[bloomFramebuffer]->bind();
                glBindTexture(GL_TEXTURE_2D, bloomBuffers[1]->getColorTextureId());
                bloomShader->set("blurDirection", blurDirectionX);
                fullscreenQuad->Draw();

                // Vertical pass
                bloomBuffers[1]->bind();
                glBindTexture(GL_TEXTURE_2D, bloomBuffers[bloomFramebuffer]->getColorTextureId());
                bloomShader->set("blurDirection", blurDirectionY);
                fullscreenQuad->Draw();
            }

            // The final result is always in bloomBuffers[1]
            bloomFramebufferResult = 1;
        }

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void ForwardRenderer::destroy()
    {
        // Delete all objects related to the sky
        if (skyMaterial)
        {
            delete skySphere;
            delete skyMaterial->shader;
            delete skyMaterial->texture;
            delete skyMaterial->sampler;
            delete skyMaterial;
        }
        // Delete all objects related to post processing
        if (postprocessMaterial)
        {
            glDeleteFramebuffers(1, &postprocessFrameBuffer);
            glDeleteVertexArrays(1, &postProcessVertexArray);
            delete colorTarget;
            delete depthTarget;
            delete postprocessMaterial->sampler;
            delete postprocessMaterial->shader;
            delete postprocessMaterial;
        }

        delete hdrSystem;
    }

    void ForwardRenderer::render(World *world)
    {
        // First of all, we search for a camera and for all the mesh renderers
        CameraComponent *camera = nullptr;
        opaqueCommands.clear();
        transparentCommands.clear();
        for (auto entity : world->getEntities())
        {
            // If we hadn't found a camera yet, we look for a camera in this entity
            if (!camera)
                camera = entity->getComponent<CameraComponent>();
            // If this entity has a mesh renderer component
            if (auto meshRenderer = entity->getComponent<MeshRendererComponent>(); meshRenderer)
            {
                // We construct a command from it
                RenderCommand command;
                command.localToWorld = meshRenderer->getOwner()->getLocalToWorldMatrix();
                command.center = glm::vec3(command.localToWorld * glm::vec4(0, 0, 0, 1));
                command.mesh = meshRenderer->mesh;
                command.material = meshRenderer->material;
                // if it is transparent, we add it to the transparent commands list
                if (command.material->transparent)
                {
                    transparentCommands.push_back(command);
                }
                else
                {
                    // Otherwise, we add it to the opaque command list
                    opaqueCommands.push_back(command);
                }
            }
        }

        // If there is no camera, we return (we cannot render without a camera)
        if (camera == nullptr)
            return;

        // TODO: (Req 9) Modify the following line such that "cameraForward" contains a vector pointing the camera forward direction
        //  HINT: See how you wrote the CameraComponent::getViewMatrix, it should help you solve this one
        auto cameraMatrix = camera->getOwner()->getLocalToWorldMatrix();
        glm::vec3 cameraForward = glm::normalize(glm::vec3(cameraMatrix[2]));
        std::sort(transparentCommands.begin(), transparentCommands.end(), [cameraForward](const RenderCommand &first, const RenderCommand &second)
                  {
            //TODO: (Req 9) Finish this function
            // HINT: the following return should return true "first" should be drawn before "second". 
            float distance1 = glm::dot(first.center, cameraForward);
            float distance2 = glm::dot(second.center, cameraForward);
            return distance1 < distance2; });

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

        // TODO: (Req 9) Set the color mask to true and the depth mask to true (to ensure the glClear will affect the framebuffer)
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);

        // If there is a postprocess material, bind the framebuffer
        if (postprocessMaterial)
        {
            // TODO: (Req 11) bind the framebuffer
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postprocessFrameBuffer);

            unsigned int colorAttachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
            glDrawBuffers(2, colorAttachments);
        }

        // TODO: (Req 9) Clear the color and depth buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //! The order of the hdrSystem is important
        if (this->hdrSystem)
        {
            // bind pre-computed IBL data
            this->hdrSystem->bindTextures();
        }

        // TODO: (Req 9) Draw all the opaque commands
        //  Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
        for (auto &command : opaqueCommands)
        {
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
        if (this->skyMaterial)
        {
            // TODO: (Req 10) setup the sky material
            this->skyMaterial->setup();

            // TODO: (Req 10) Get the camera position
            glm::vec3 cameraPosition = camera->getOwner()->localTransform.position;

            // TODO: (Req 10) Create a model matrix for the sky such that it always follows the camera (sky sphere center = camera position)
            our::Transform skyTransformer;
            skyTransformer.position = cameraPosition;
            glm::mat4 skyModel = skyTransformer.toMat4();

            // TODO: (Req 10) We want the sky to be drawn behind everything (in NDC space, z=1)
            //  We can acheive the is by multiplying by an extra matrix after the projection but what values should we put in it?
            glm::mat4 alwaysBehindTransform = glm::mat4(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 1.0f);

            // TODO: (Req 10) set the "transform" uniform
            glm::mat4 finalTransform = alwaysBehindTransform * VP * skyModel;
            this->skyMaterial->shader->set("transform", finalTransform);

            // TODO: (Req 10) draw the sky sphere
            this->skySphere->draw();
        }

        // TODO: (Req 9) Draw all the transparent commands
        //  Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
        for (auto &command : transparentCommands)
        {
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
        
        //! The order of the hdrSystem is important
        if (this->hdrSystem)
        {
            // Render the background if the HDR system is enabled
            hdrSystem->renderBackground(projection, view, 10);
        }
        
        // If there is a postprocess material, apply postprocessing
        if (postprocessMaterial)
        {

            renderBloom();
            // TODO: (Req 11) Return to the default framebuffer
            glViewport(0, 0, windowSize.x, windowSize.y);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            // TODO: (Req 11) Setup the postprocess material and draw the fullscreen triangle
            postprocessMaterial->setup();
            postprocessMaterial->shader->set("bloomEnabled", bloomEnabled);
            postprocessMaterial->shader->set("bloomIntensity", bloomIntensity);
            postprocessMaterial->shader->set("tonemappingEnabled", tonemappingEnabled);
            postprocessMaterial->shader->set("gammaCorrectionFactor", gammaCorrectionFactor);
            
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, colorTarget->getOpenGLName());
            postprocessMaterial->shader->set("colorTexture", 0);
            
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, bloomBuffers[bloomFramebufferResult]->getColorTextureId());
            postprocessMaterial->shader->set("bloomTexture", 1);
            
            // glBindVertexArray(postProcessVertexArray);
            // glDrawArrays(GL_TRIANGLES, 0, 3);
            fullscreenQuad->Draw();
        }

        
    }
    
}