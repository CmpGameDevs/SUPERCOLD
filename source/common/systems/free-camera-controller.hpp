#pragma once

#include "../ecs/world.hpp"
#include "../components/camera.hpp"
#include "../components/free-camera-controller.hpp"

#include "../application.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtx/fast_trigonometry.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace our
{

    // The free camera controller system is responsible for moving every entity which contains a FreeCameraControllerComponent.
    // This system is added as a slightly complex example for how use the ECS framework to implement logic. 
    // For more information, see "common/components/free-camera-controller.hpp"
    class FreeCameraControllerSystem {
        Application* app; // The application in which the state runs
        bool mouse_locked = false; // Is the mouse locked

    public:
        // When a state enters, it should call this function and give it the pointer to the application
        void enter(Application* app){
            this->app = app;
        }

        // This should be called every frame to update all entities containing a FreeCameraControllerComponent 
        void update(World* world, float deltaTime) {
            CameraComponent* camera = nullptr;
            FreeCameraControllerComponent* controller = nullptr;
        
            for (auto entity : world->getEntities()) {
                camera = entity->getComponent<CameraComponent>();
                controller = entity->getComponent<FreeCameraControllerComponent>();
                if (camera && controller) break;
            }
        
            if (!(camera && controller)) return;
        
            Entity* entity = camera->getOwner();
            glm::vec3& position = entity->localTransform.position;
        
            // Handle mouse locking/unlocking
            if (app->getMouse().isPressed(GLFW_MOUSE_BUTTON_1) && !mouse_locked) {
                app->getMouse().lockMouse(app->getWindow());
                mouse_locked = true;
            } else if (!app->getMouse().isPressed(GLFW_MOUSE_BUTTON_1) && mouse_locked) {
                app->getMouse().unlockMouse(app->getWindow());
                mouse_locked = false;
            }
        
            // Handle rotation
            if (app->getMouse().isPressed(GLFW_MOUSE_BUTTON_1)) {
                glm::vec2 delta = app->getMouse().getMouseDelta();
        
                controller->pitch -= delta.y * controller->rotationSensitivity;
                controller->yaw   -= delta.x * controller->rotationSensitivity;
        
                // Clamp pitch to prevent flipping
                float pitchLimit = glm::half_pi<float>() * 0.99f;
                controller->pitch = glm::clamp(controller->pitch, -pitchLimit, pitchLimit);
                controller->yaw = glm::wrapAngle(controller->yaw);
        
                // Convert to quaternion rotation
                glm::quat qPitch = glm::angleAxis(controller->pitch, glm::vec3(1, 0, 0));
                glm::quat qYaw   = glm::angleAxis(controller->yaw, glm::vec3(0, 1, 0));
                entity->localTransform.rotation = qYaw * qPitch;
            }
        
            // Update camera FOV based on mouse wheel
            float fov = camera->fovY + app->getMouse().getScrollOffset().y * controller->fovSensitivity;
            camera->fovY = glm::clamp(fov, glm::pi<float>() * 0.01f, glm::pi<float>() * 0.99f);
        
            // Calculate directions
            glm::mat4 matrix = entity->localTransform.toMat4();
            glm::vec3 front  = glm::vec3(matrix * glm::vec4(0, 0, -1, 0));
            glm::vec3 up     = glm::vec3(matrix * glm::vec4(0, 1, 0, 0));
            glm::vec3 right  = glm::vec3(matrix * glm::vec4(1, 0, 0, 0));
        
            glm::vec3 sensitivity = controller->positionSensitivity;
            if (app->getKeyboard().isPressed(GLFW_KEY_LEFT_SHIFT))
                sensitivity *= controller->speedupFactor;
        
            // Movement input
            if (app->getKeyboard().isPressed(GLFW_KEY_W)) position += front * (deltaTime * sensitivity.z);
            if (app->getKeyboard().isPressed(GLFW_KEY_S)) position -= front * (deltaTime * sensitivity.z);
            if (app->getKeyboard().isPressed(GLFW_KEY_Q) || app->getKeyboard().isPressed(GLFW_KEY_SPACE)) position += up * (deltaTime * sensitivity.y);
            if (app->getKeyboard().isPressed(GLFW_KEY_E)) position -= up * (deltaTime * sensitivity.y);
            if (app->getKeyboard().isPressed(GLFW_KEY_D)) position += right * (deltaTime * sensitivity.x);
            if (app->getKeyboard().isPressed(GLFW_KEY_A)) position -= right * (deltaTime * sensitivity.x);
        }
        

        // When the state exits, it should call this function to ensure the mouse is unlocked
        void exit(){
            if(mouse_locked) {
                mouse_locked = false;
                app->getMouse().unlockMouse(app->getWindow());
            }
        }

    };

}
