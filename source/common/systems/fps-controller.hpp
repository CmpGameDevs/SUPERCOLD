#pragma once
#include "../application.hpp"
#include "../components/camera.hpp"
#include "../components/fps-controller.hpp"
#include "../ecs/world.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/fast_trigonometry.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace our {

// =========================================================================
// FPSControllerSystem
//
// Controls:
//   - W/A/S/D:   Movement
//   - Mouse:     Rotation (Yaw and Pitch)
//   - ESC:       Toggle Mouse Lock/Unlock (Exit)
//   - Left Shift: Sprint (Consumes Stamina)
//   - Space:     Jump
//   - C:         Crouch (No functionality implemented in this version)
//   NOTE: Q/E are left for debugging but will be removed.
//   - Q/E:       Vertical Movement (Up/Down) - only when not jumping and mouse locked
// =========================================================================

template <typename T> T clamp(T value, T min, T max) {
    return std::max(min, std::min(value, max));
}

class FPSControllerSystem {
  public:
    static constexpr float NEAR_ZERO = 0.001f;

  private:
    Application *app = nullptr;
    bool mouseLocked = true;
    float jumpTimer = 0.0f;
    glm::vec3 currentVelocity = glm::vec3(0.0f);
    float verticalVelocity = 0.0f;
    bool isGrounded = true;

    // Helper function to find the controlled entity
    std::pair<CameraComponent *, FPSControllerComponent *> findControlledEntity(World *world) {
        for (auto entity : world->getEntities()) {
            auto camera = entity->getComponent<CameraComponent>();
            auto controller = entity->getComponent<FPSControllerComponent>();
            if (camera && controller) {
                return {camera, controller};
            }
        }
        return {nullptr, nullptr};
    }

    // Handles mouse input for rotation
    void handleRotation(FPSControllerComponent *controller) {
        if (!mouseLocked) return;
    
        glm::vec2 delta = app->getMouse().getMouseDelta();
        
        float pitchDelta = delta.y * controller->rotationSensitivityY;
        float yawDelta = delta.x * controller->rotationSensitivityX;
    
        if (controller->invertYAxis)
            pitchDelta = -pitchDelta;
    
        controller->pitch = glm::clamp(
            controller->pitch - pitchDelta,
            glm::radians(controller->minVerticalRotation),
            glm::radians(controller->maxVerticalRotation)
        );
    
        controller->yaw -= yawDelta;
    
        // Create rotation quaternion
        glm::quat qPitch = glm::angleAxis(controller->pitch, glm::vec3(1, 0, 0));
        glm::quat qYaw = glm::angleAxis(controller->yaw, glm::vec3(0, 1, 0));
    
        // Final rotation: yaw first (world Y), then pitch (local X)
        controller->getOwner()->localTransform.rotation = qYaw * qPitch;
    }

    // Handles FOV adjustment with mouse wheel
    void handleFov(CameraComponent *camera, FPSControllerComponent *controller) {
        float fov = camera->fovY + app->getMouse().getScrollOffset().y * controller->fovSensitivity;
        camera->fovY = clamp(fov, glm::pi<float>() * 0.01f, glm::pi<float>() * 0.99f);
    }

    // Handles toggling of mouse lock
    void handleMouseLockToggle() {
        if (app->getKeyboard().justPressed(GLFW_KEY_ESCAPE)) {
            if (mouseLocked) {
                app->getMouse().unlockMouse(app->getWindow());
            } else {
                app->getMouse().lockMouse(app->getWindow());
            }
            mouseLocked = !mouseLocked;
        }
    }

    // Handles sprinting and stamina
    void handleStamina(FPSControllerComponent *controller, float deltaTime) {
        if (app->getKeyboard().isPressed(GLFW_KEY_LEFT_SHIFT) && controller->currentStamina > 0) {
            controller->isSprinting = true;
            controller->currentStamina -= controller->staminaDepletionRate * deltaTime;
            controller->currentStamina = std::max(controller->currentStamina, 0.0f);
        } else {
            controller->isSprinting = false;
            controller->currentStamina += controller->staminaRecoveryRate * deltaTime;
            controller->currentStamina = std::min(controller->currentStamina, controller->maxStamina);
        }
    }

    // Handles jumping mechanics
    void handleJump(FPSControllerComponent *controller, float deltaTime, glm::vec3 &position) {
        if (isGrounded && jumpTimer <= 0.0f && app->getKeyboard().justPressed(GLFW_KEY_SPACE)) {
            verticalVelocity = sqrt(2.0f * controller->gravity * controller->jumpHeight);
            isGrounded = false;
            controller->isJumping = true;
            jumpTimer = controller->jumpCooldown;
        } else {
            jumpTimer -= deltaTime;

            if (!isGrounded) {
                verticalVelocity -= controller->gravity * deltaTime;
                position.y += verticalVelocity * deltaTime;

                if (position.y <= 0.0f) {
                    position.y = 0.0f;
                    verticalVelocity = 0.0f;
                    isGrounded = true;
                    controller->isJumping = false;
                }
            }
        }
    }

    // Handles movement based on input
    glm::vec3 handleMovement(FPSControllerComponent *controller, float deltaTime) {
        glm::vec3 movementDirection(0.0f);

        glm::mat4 matrix = controller->getOwner()->localTransform.toMat4();
        glm::vec3 front = glm::vec3(matrix * glm::vec4(0, 0, -1, 0));
        glm::vec3 right = glm::vec3(matrix * glm::vec4(1, 0, 0, 0));

        if (app->getKeyboard().isPressed(GLFW_KEY_W))
            movementDirection += front;
        if (app->getKeyboard().isPressed(GLFW_KEY_S))
            movementDirection -= front;
        if (app->getKeyboard().isPressed(GLFW_KEY_D))
            movementDirection += right;
        if (app->getKeyboard().isPressed(GLFW_KEY_A))
            movementDirection -= right;

        if (glm::length(movementDirection) > NEAR_ZERO) {
            movementDirection = glm::normalize(movementDirection);
        }

        return movementDirection;
    }

    // Applies movement smoothing
    void applyMovementSmoothing(FPSControllerComponent *controller, const glm::vec3 &movementDirection,
                                float deltaTime) {
        // Calculate movement speed based on current state
        glm::vec3 currentSensitivity = glm::vec3(controller->positionSensitivity);

        // Apply sprinting speed modifier
        if (controller->isSprinting) {
            currentSensitivity *= controller->sprintSpeedModifier;
        }

        // Apply crouching speed modifier
        if (controller->isCrouching) {
            currentSensitivity *= controller->crouchSpeedModifier;
        }

        glm::vec3 targetVelocity = movementDirection * currentSensitivity;

        if (glm::length(movementDirection) > NEAR_ZERO) {
            currentVelocity =
                glm::mix(currentVelocity, targetVelocity, std::min(deltaTime * controller->acceleration, 1.0f));
        } else {
            currentVelocity =
                glm::mix(currentVelocity, glm::vec3(0.0f), std::min(deltaTime * controller->deceleration, 1.0f));
        }
    }

  public:
    // Enters the state and locks the mouse
    void enter(Application *app) {
        this->app = app;
        app->getMouse().lockMouse(app->getWindow());
    }

    float getSpeedMagnitude() {
        return glm::length(currentVelocity);
    }
    
    // Updates the FPS controller every frame
    void update(World *world, float deltaTime) {
        auto [camera, controller] = findControlledEntity(world);

        if (!(camera && controller))
            return;

        Entity *entity = camera->getOwner();
        glm::vec3 &position = entity->localTransform.position;

        handleMouseLockToggle();
        handleFov(camera, controller);
        handleStamina(controller, deltaTime);
        handleJump(controller, deltaTime, position);

        // Handle crouch toggling with C key
        if (app->getKeyboard().justPressed(GLFW_KEY_C)) {
            controller->isCrouching = !controller->isCrouching;
        }

        glm::vec3 movementDirection = handleMovement(controller, deltaTime);
        applyMovementSmoothing(controller, movementDirection, deltaTime);

        // Apply horizontal movement
        position += glm::vec3(currentVelocity.x, 0, currentVelocity.z) * deltaTime;

        handleRotation(controller);

        // Handle vertical movement based on Q & E keys (only when not jumping and
        // mouse is locked)
        if (isGrounded && mouseLocked) {
            glm::mat4 matrix = controller->getOwner()->localTransform.toMat4();
            glm::vec3 up = glm::vec3(matrix * glm::vec4(0, 1, 0, 0));
            glm::vec3 currentSensitivity = glm::vec3(controller->positionSensitivity);

            if (app->getKeyboard().isPressed(GLFW_KEY_Q))
                position += up * (deltaTime * currentSensitivity.y);
            if (app->getKeyboard().isPressed(GLFW_KEY_E))
                position -= up * (deltaTime * currentSensitivity.y);
        }
    }

    // Unlocks the mouse when the state exits
    void exit() {
        if (mouseLocked) {
            mouseLocked = false;
            app->getMouse().unlockMouse(app->getWindow());
        }
    }
};
} // namespace our
