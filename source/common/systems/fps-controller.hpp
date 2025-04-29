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
#include "collision-system.hpp"
#include "weapons-system.hpp"

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
// =========================================================================

template <typename T> T clamp(T value, T min, T max) {
    return std::max(min, std::min(value, max));
}

class FPSControllerSystem {
  public:
    static constexpr float NEAR_ZERO = 0.001f;
    static constexpr float GROUND_EPSILON = 0.3f;

  private:
    Application *app = nullptr;
    CollisionSystem *collisionSystem = nullptr;
    bool mouseLocked = true;
    float jumpTimer = 0.0f;
    glm::vec3 currentVelocity = glm::vec3(0.0f);
    glm::vec3 timeScaleVelocity = glm::vec3(0.0f);
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

    bool checkGrounded(FPSControllerComponent *controller, Entity* entity) {
        bool isGrounded = false;
        if (collisionSystem != nullptr) {
            CollisionComponent* collision = entity->getComponent<CollisionComponent>();
            if (collision) {
                btTransform transform;
                if (collision->bulletBody) transform = collision->bulletBody->getWorldTransform();
                else if (collision->ghostObject) transform = collision->ghostObject->getWorldTransform();
                else return false;

                glm::vec3 ghostPosition(
                    transform.getOrigin().x(),
                    transform.getOrigin().y(),
                    transform.getOrigin().z()
                );

                float height = controller->currentGhostHeight * 0.5f;
                glm::vec3 rayStart = ghostPosition;
                glm::vec3 rayEnd = ghostPosition - glm::vec3(0, height + GROUND_EPSILON, 0);
                CollisionComponent* hitComponent = nullptr;
                glm::vec3 hitPoint, hitNormal;

                glm::vec3 color(0.0f, 1.0f, 1.0f); // Cyan color
                if (collisionSystem->raycast(rayStart, rayEnd, hitComponent, hitPoint, hitNormal)) {
                    isGrounded = true;
                    verticalVelocity = 0.0f;
                    controller->isJumping = false;
                    color = glm::vec3(1.0f, 0.5f, 0.0f); // Orange color
                }
                collisionSystem->debugDrawRay(rayStart, rayEnd, color);
            }
        }
        return isGrounded;
    }

    // Handles jumping mechanics
    void handleJump(FPSControllerComponent *controller, float deltaTime) {
        btKinematicCharacterController* characterController = controller->characterController;
        if (app->getKeyboard().justPressed(GLFW_KEY_SPACE) && isGrounded) {
            float jumpHeight = controller->currentGhostHeight + controller->jumpHeight;
            verticalVelocity = sqrt(2.0f * controller->gravity * jumpHeight);
            characterController->setMaxJumpHeight(jumpHeight);
            characterController->setJumpSpeed(verticalVelocity);
            characterController->jump();
            controller->isJumping = true;
        } else if (isGrounded) {
            controller->isJumping = false;
            verticalVelocity = 0.0f;
        } else {
            verticalVelocity -= controller->gravity * deltaTime;
        }
    }

    // Handles movement based on input
    glm::vec3 handleMovement(const FPSControllerComponent *controller, const float deltaTime) const {
        glm::vec3 movementDirection(0.0f);

        glm::mat4 matrix = controller->getOwner()->localTransform.toMat4();
        glm::vec3 front = glm::vec3(matrix * glm::vec4(0, 0, -1, 0));
        front.y = 0.0f;
        front = glm::normalize(front);

        glm::vec3 right = glm::vec3(matrix * glm::vec4(1, 0, 0, 0));
        right.y = 0.0f;
        right = glm::normalize(right);

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
    void applyMovementSmoothing(const FPSControllerComponent *controller, const glm::vec3 &movementDirection,
                                const float deltaTime) {
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

    void handleCrouching(FPSControllerComponent *controller, Entity *entity, float deltaTime) {
        if (isGrounded && app->getKeyboard().justPressed(GLFW_KEY_C)) {
            controller->isCrouching = !controller->isCrouching;
            controller->isCrouchTransitioning = true;
            controller->crouchLerpProgress = 0.0f;
        
            controller->initialGhostHeight = controller->currentGhostHeight;
            controller->targetGhostHeight = controller->isCrouching 
                ? controller->height * controller->crouchHeightModifier 
                : controller->height;
        }

        if (controller->isCrouchTransitioning) {
            controller->crouchLerpProgress += deltaTime / controller->crouchTransitionTime;
            if (controller->crouchLerpProgress >= 1.0f) {
                controller->crouchLerpProgress = 1.0f;
                controller->isCrouchTransitioning = false;
            }
        
            if (auto collision = entity->getComponent<CollisionComponent>()) {
                if (collision->ghostObject) {
                    float newHeight = glm::mix(
                        controller->initialGhostHeight,
                        controller->targetGhostHeight,
                        controller->crouchLerpProgress
                    );
        
                    // resize the shape
                    float halfHeight = newHeight * 0.5f;
                    btCapsuleShape* capsule = static_cast<btCapsuleShape*>(collision->ghostObject->getCollisionShape());
                    collision->halfExtents.y = halfHeight;
                    btVector3 oldScale = capsule->getLocalScaling();
                    capsule->setLocalScaling(btVector3(
                        oldScale.x(),
                        oldScale.y(),
                        oldScale.z() * (newHeight / controller->currentGhostHeight)
                    ));

                    // reposition so the bottom of the capsule is at the same height as before
                    btTransform t = collision->ghostObject->getWorldTransform();
                    float oldHalfHeight = controller->currentGhostHeight * 0.5f;
                    float bottomY = t.getOrigin().y() - oldHalfHeight;
                    t.getOrigin().setY(bottomY + halfHeight);
                    collision->ghostObject->setWorldTransform(t);

                    auto* world = collisionSystem->getPhysicsWorld();
                    world->getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(
                    collision->ghostObject->getBroadphaseHandle(),
                    world->getDispatcher()
                    );
                    world->updateSingleAabb(collision->ghostObject);
                    controller->currentGhostHeight = newHeight;
                }
            }
        }
    }

    void updateTimeScaleVelocity(FPSControllerComponent *controller) {
        timeScaleVelocity = glm::vec3(currentVelocity.x, verticalVelocity, currentVelocity.z);
        const float EPSILON = 0.2f;
        bool isMoving = glm::length(timeScaleVelocity) > EPSILON;
        if (controller->isSprinting || controller->isJumping) timeScaleVelocity = (isMoving ? timeScaleVelocity : glm::vec3(1,1,1)) * controller->actionSpeedModifier;
        if (controller->isCrouching) timeScaleVelocity *= controller->crouchSpeedModifier;
    }

    void handlePickup(FPSControllerComponent *controller, Entity *entity) {
        if (app->getKeyboard().justPressed(GLFW_KEY_E)) {
            if (collisionSystem == nullptr) return;
            CollisionComponent *collision = entity->getComponent<CollisionComponent>();
            if (!collision || !collision->ghostObject) return;
            btTransform transform;
            if (collision->ghostObject) transform = collision->ghostObject->getWorldTransform();

            glm::vec3 ghostPosition(
                transform.getOrigin().x(),
                transform.getOrigin().y(),
                transform.getOrigin().z()
            );

            glm::vec3 rayStart = ghostPosition;
            // Ray should point forward from the camera
            auto cameraMatrix = entity->getLocalToWorldMatrix();
            glm::vec3 cameraForward = -glm::normalize(glm::vec3(cameraMatrix[2]));
            glm::vec3 rayEnd = rayStart + cameraForward * controller->pickupDistance;
            CollisionComponent *hitComponent = nullptr;
            glm::vec3 hitPoint, hitNormal;

            glm::vec3 color(0.0f, 1.0f, 1.0f); // Cyan color
            if (collisionSystem->raycast(rayStart, rayEnd, hitComponent, hitPoint, hitNormal)) {
                auto otherEntity = hitComponent->getOwner();
                if (controller->pickedEntity)
                    WeaponsSystem::getInstance().dropWeapon(entity->getWorld(), controller->pickedEntity);
                controller->pickedEntity = otherEntity;
                WeaponsSystem::getInstance().pickupWeapon(entity->getWorld(), entity, otherEntity);
                color = glm::vec3(1.0f, 0.5f, 0.0f); // Orange color
            }
            collisionSystem->debugDrawRay(rayStart, rayEnd, color);
        } else if (app->getKeyboard().justPressed(GLFW_KEY_Q)) {
            if (controller->pickedEntity) {
                auto cameraMatrix = entity->getLocalToWorldMatrix();
                glm::vec3 cameraForward = -glm::normalize(glm::vec3(cameraMatrix[2]));
                WeaponsSystem::getInstance().throwWeapon(entity->getWorld(), controller->pickedEntity, cameraForward);
                controller->pickedEntity = nullptr;
            }
        }
    }

public:
    // Enters the state and locks the mouse
    void enter(Application *app) {
        this->app = app;
        app->getMouse().lockMouse(app->getWindow());
    }

    void setCollisionSystem(CollisionSystem *collisionSystem) {
        this->collisionSystem = collisionSystem;
    }

    float getSpeedMagnitude() {
        return glm::length(timeScaleVelocity);
    }
    
    // Updates the FPS controller every frame
    void update(World *world, float deltaTime) {
        auto [camera, controller] = findControlledEntity(world);
        if (!(camera && controller)) return;
        auto characterController = controller->characterController;
        if (!characterController) return;

        Entity *entity = camera->getOwner();
        glm::vec3 &position = entity->localTransform.position;
        isGrounded = characterController->onGround();

        // Handle crouch toggling with C key
        handleCrouching(controller, entity, deltaTime);
        handlePickup(controller, entity);

        handleMouseLockToggle();
        handleFov(camera, controller);
        handleStamina(controller, deltaTime);

        glm::vec3 movementDirection = handleMovement(controller, deltaTime);
        applyMovementSmoothing(controller, movementDirection, deltaTime);
        handleJump(controller, deltaTime);

        glm::vec3 horizontalVelocity = glm::vec3(currentVelocity.x, 0.0f, currentVelocity.z);
        glm::vec3 totalMovement = horizontalVelocity * deltaTime;

        collisionSystem->moveGhost(entity, totalMovement, deltaTime);

        updateTimeScaleVelocity(controller);
        handleRotation(controller);
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
