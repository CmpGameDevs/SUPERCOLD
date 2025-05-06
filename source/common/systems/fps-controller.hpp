#pragma once
#include "collision-system.hpp"
#include "weapons-system.hpp"
#include <application.hpp>
#include <components/camera.hpp>
#include <components/collision.hpp>
#include <components/crosshair.hpp>
#include <components/fps-controller.hpp>
#include <components/weapon.hpp>
#include <ecs/world.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/fast_trigonometry.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <systems/text-renderer.hpp>

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
    bool mouseLocked = true;
    float jumpTimer = 0.0f;
    glm::vec3 currentVelocity = glm::vec3(0.0f);
    glm::vec3 timeScaleVelocity = glm::vec3(0.0f);
    float verticalVelocity = 0.0f;
    bool isGrounded = true;
    float timeStandingStill = 0.0f;
    bool moved = false;
    FPSControllerComponent *controller = nullptr;
    CameraComponent *camera = nullptr;

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
        if (!mouseLocked)
            return;

        glm::vec2 delta = app->getMouse().getMouseDelta();

        float pitchDelta = delta.y * controller->rotationSensitivityY;
        float yawDelta = delta.x * controller->rotationSensitivityX;

        if (controller->invertYAxis)
            pitchDelta = -pitchDelta;

        controller->pitch = glm::clamp(controller->pitch - pitchDelta, glm::radians(controller->minVerticalRotation),
                                       glm::radians(controller->maxVerticalRotation));

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
    void handleJump(FPSControllerComponent *controller, float deltaTime) {
        if(deltaTime <= 0) return;
        btKinematicCharacterController *characterController = controller->characterController.get();
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
    glm::vec3 handleMovement(const FPSControllerComponent *controller, const float deltaTime) {
        glm::vec3 movementDirection(0.0f);
        if(deltaTime <= 0) return movementDirection;

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
            moved = true;
            movementDirection = glm::normalize(movementDirection);
        }

        return movementDirection;
    }

    // Applies movement smoothing
    void applyMovementSmoothing(const FPSControllerComponent *controller, const glm::vec3 &movementDirection,
                                const float deltaTime) {
        if(deltaTime <= 0) return;
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
        if(deltaTime <= 0) return;
        if (isGrounded && app->getKeyboard().justPressed(GLFW_KEY_C)) {
            controller->isCrouching = !controller->isCrouching;
            controller->isCrouchTransitioning = true;
            controller->crouchLerpProgress = 0.0f;

            controller->initialGhostHeight = controller->currentGhostHeight;
            controller->targetGhostHeight =
                controller->isCrouching ? controller->height * controller->crouchHeightModifier : controller->height;
        }

        if (controller->isCrouchTransitioning) {
            std::cout << "Crouch lerp progress: " << controller->crouchLerpProgress << std::endl;
            controller->crouchLerpProgress += deltaTime / controller->crouchTransitionTime;
            if (controller->crouchLerpProgress >= 1.0f) {
                controller->crouchLerpProgress = 1.0f;
                controller->isCrouchTransitioning = false;
            }

            if (auto collision = entity->getComponent<CollisionComponent>()) {
                if (collision->ghostObject) {
                    float newHeight = glm::mix(controller->initialGhostHeight, controller->targetGhostHeight,
                                               controller->crouchLerpProgress);

                    // resize the shape
                    float halfHeight = newHeight * 0.5f;
                    btCapsuleShape *capsule =
                        static_cast<btCapsuleShape *>(collision->ghostObject->getCollisionShape());
                    collision->halfExtents.y = halfHeight;
                    btVector3 oldScale = capsule->getLocalScaling();
                    capsule->setLocalScaling(btVector3(oldScale.x(), oldScale.y(),
                                                       oldScale.z() * (newHeight / controller->currentGhostHeight)));

                    // reposition so the bottom of the capsule is at the same height as before
                    btTransform t = collision->ghostObject->getWorldTransform();
                    float oldHalfHeight = controller->currentGhostHeight * 0.5f;
                    float bottomY = t.getOrigin().y() - oldHalfHeight;
                    t.getOrigin().setY(bottomY + halfHeight);
                    collision->ghostObject->setWorldTransform(t);

                    auto *world = CollisionSystem::getInstance().getPhysicsWorld();
                    world->getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(
                        collision->ghostObject->getBroadphaseHandle(), world->getDispatcher());
                    world->updateSingleAabb(collision->ghostObject);
                    controller->currentGhostHeight = newHeight;
                    std::cout << "New ghost height: " << controller->currentGhostHeight << std::endl;
                    std::cout << "Is Crouching" << controller->isCrouching << std::endl;
                }
            }
        }
    }

    void handleHeadBob(FPSControllerComponent *controller, Entity *entity, float deltaTime, bool isMoving) {
        // Update the bob timer
        controller->bobTimer +=
            deltaTime * (isMoving
                             ? (controller->isSprinting ? controller->headBobFrequency * controller->sprintBobMultiplier
                                                        : controller->headBobFrequency)
                             : controller->idleBobFrequency);

        // Don't apply effects while jumping
        if (controller->isJumping || controller->isCrouchTransitioning || controller->isCrouching)
            return;

        // Calculate base height from crouch state
        float baseHeight =
            controller->isCrouching ? controller->height * controller->crouchHeightModifier : controller->height;

        float verticalOffset = 0.0f;
        if (!controller->isCrouchTransitioning) {
            if (isMoving) {
                float amplitude =
                    controller->headBobAmplitude * (controller->isSprinting ? controller->sprintBobMultiplier : 1.0f);
                verticalOffset = sin(controller->bobTimer) * amplitude;
            } else {
                verticalOffset = sin(controller->bobTimer) * controller->idleBobAmplitude;
            }
        }

        // Apply the height and bob offset to the character controller and ghost object
        if (auto collision = entity->getComponent<CollisionComponent>()) {
            if (collision->ghostObject) {
                btCapsuleShape *capsule = static_cast<btCapsuleShape *>(collision->ghostObject->getCollisionShape());

                btTransform currentTransform = collision->ghostObject->getWorldTransform();
                btVector3 currentPosition = currentTransform.getOrigin();
                float currentBottom = currentPosition.y() - (controller->currentGhostHeight * 0.5f);

                float finalHeight = baseHeight + verticalOffset;

                // Update the capsule shape
                float halfHeight = finalHeight * 0.5f;
                collision->halfExtents.y = halfHeight;
                capsule->setLocalScaling(btVector3(1.0f, finalHeight / controller->height, 1.0f));

                // Update the ghost object position to maintain the same bottom position
                currentPosition.setY(currentBottom + halfHeight);
                currentTransform.setOrigin(currentPosition);
                collision->ghostObject->setWorldTransform(currentTransform);

                // Update the physics world
                auto *world = CollisionSystem::getInstance().getPhysicsWorld();
                world->getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(
                    collision->ghostObject->getBroadphaseHandle(), world->getDispatcher());
                world->updateSingleAabb(collision->ghostObject);

                // Update the current ghost height
                controller->currentGhostHeight = finalHeight;
            }
        }
    }

    void updateTimeScaleVelocity(FPSControllerComponent *controller, float deltaTime) {
        if(deltaTime <= 0) return;
        timeScaleVelocity = glm::vec3(currentVelocity.x, verticalVelocity, currentVelocity.z);
        const float EPSILON = 0.2f;
        bool isMoving = glm::length(timeScaleVelocity) > EPSILON;
        if (controller->isSprinting || controller->isJumping)
            timeScaleVelocity = (isMoving ? timeScaleVelocity : glm::vec3(1, 1, 1)) * controller->actionSpeedModifier;
        if (controller->isCrouching)
            timeScaleVelocity *= controller->crouchSpeedModifier;
    }

    void updateStillTimer(const glm::vec3 &movementDirection, float originalDeltaTime) {
        if (!moved)
            return;
        if (glm::length(movementDirection) < NEAR_ZERO) {
            timeStandingStill += originalDeltaTime;
        } else {
            timeStandingStill = 0.0f;
        }
    }

    glm::vec2 worldToScreenPosition(glm::vec3 worldPos, glm::mat4 viewMatrix, glm::mat4 projectionMatrix,
                                    glm::vec2 windowSize) {
        // Will only return half the screen size, but I did it in case
        glm::vec4 clipSpacePos = projectionMatrix * viewMatrix * glm::vec4(worldPos, 1.0f);
        glm::vec3 ndcSpacePos = glm::vec3(clipSpacePos) / clipSpacePos.w;
        return glm::vec2((ndcSpacePos.x + 1.0f) * 0.5f * windowSize.x, (1.0f - ndcSpacePos.y) * 0.5f * windowSize.y);
    }

    void handlePickup(FPSControllerComponent *controller, Entity *entity, glm::mat4 viewMatrix,
                      glm::mat4 projectionMatrix) {
        CollisionSystem *collisionSystem = &CollisionSystem::getInstance();
        TextRenderer &textRenderer = TextRenderer::getInstance();

        auto performRaycast = [&]() -> std::tuple<bool, glm::vec3, WeaponComponent *> {
            CollisionComponent *collision = entity->getComponent<CollisionComponent>();
            if (!collision || !collision->ghostObject)
                return {false, {}, nullptr};

            btTransform transform = collision->ghostObject->getWorldTransform();
            glm::vec3 ghostPosition(transform.getOrigin().x(), transform.getOrigin().y(), transform.getOrigin().z());
            glm::vec3 rayStart = ghostPosition;

            auto cameraMatrix = entity->getLocalToWorldMatrix();
            glm::vec3 cameraForward = -glm::normalize(glm::vec3(cameraMatrix[2]));
            glm::vec3 rayEnd = rayStart + cameraForward * controller->pickupDistance;

            CollisionComponent *hitComponent = nullptr;
            glm::vec3 hitPoint, hitNormal;

            if (!collisionSystem->raycast(rayStart, rayEnd, hitComponent, hitPoint, hitNormal))
                return {false, {}, nullptr};

            auto otherEntity = hitComponent->getOwner();
            WeaponComponent *weapon = otherEntity->getComponent<WeaponComponent>();
            return {weapon != nullptr, hitPoint, weapon};
        };

        if (app->getKeyboard().justPressed(GLFW_KEY_E)) {
            auto [hasWeapon, hitPoint, weapon] = performRaycast();
            if (!hasWeapon)
                return;

            if (controller->pickedEntity && WeaponsSystem::getInstance().dropWeapon(controller->pickedEntity)) {
                controller->pickedEntity = nullptr;
                Crosshair::getInstance()->setWeaponHeld(false);
            }

            if (weapon && WeaponsSystem::getInstance().pickupWeapon(entity, weapon->getOwner())) {
                controller->pickedEntity = weapon->getOwner();
                Crosshair::getInstance()->setWeaponHeld(true);
            }
        } else if (app->getKeyboard().justPressed(GLFW_KEY_Q)) {
            if (controller->pickedEntity) {
                auto cameraMatrix = entity->getLocalToWorldMatrix();
                glm::vec3 cameraForward = -glm::normalize(glm::vec3(cameraMatrix[2]));
                if (WeaponsSystem::getInstance().throwWeapon(controller->pickedEntity, cameraForward)) {
                    controller->pickedEntity = nullptr;
                    Crosshair::getInstance()->setWeaponHeld(false);
                }
            }
        }

        if (!controller->pickedEntity) {
            auto [hasWeapon, hitPoint, weapon] = performRaycast();
            if (hasWeapon) {
                glm::vec2 screenPos =
                    worldToScreenPosition(hitPoint, viewMatrix, projectionMatrix, app->getWindowSize());

                textRenderer.renderTextWithBackground("E   PICKUP WEAPON", "window", screenPos.x, screenPos.y + 50.0f,
                                                      0.011f, 0.008f, glm::vec4(1.0f),
                                                      glm::vec4(0.0f, 0.0f, 0.0f, 0.6f), true);
            }
        }
    }

    std::pair<glm::mat4, glm::mat4> getViewAndProjectionMatrix(Entity *entity) {
        if (!camera || !entity)
            return {glm::mat4(1.0f), glm::mat4(1.0f)};
        auto cameraMatrix = entity->getLocalToWorldMatrix();
        auto viewMatrix = glm::inverse(cameraMatrix);
        printf("Camera matrix: %f %f %f %f\n", cameraMatrix[0][0], cameraMatrix[1][0], cameraMatrix[2][0],
               cameraMatrix[3][0]);
        auto windowSize = app->getWindowSize();
        printf("Window size: %f %f\n", windowSize.x, windowSize.y);
        glm::vec2 viewPortSize = glm::vec2(windowSize.x, windowSize.y);
        printf("View port size: %f %f\n", viewPortSize.x, viewPortSize.y);
        auto projectionMatrix = camera->getProjectionMatrix(viewPortSize);
        return {viewMatrix, projectionMatrix};
    }

    void handleShooting(FPSControllerComponent *controller, Entity *entity, float deltaTime, glm::mat4 viewMatrix,
                        glm::mat4 projectionMatrix) {
        if (!controller->pickedEntity)
            return;
        auto weapon = controller->pickedEntity->getComponent<WeaponComponent>();
        if (!weapon)
            return;
        weapon->fireCooldown -= deltaTime;
        bool isShooting = app->getMouse().justPressed(GLFW_MOUSE_BUTTON_LEFT) ||
                          (app->getMouse().isPressed(GLFW_MOUSE_BUTTON_LEFT) && weapon->automatic);
        if (isShooting) {
            glm::vec3 cameraForward = -glm::normalize(glm::vec3(entity->getLocalToWorldMatrix()[2]));

            WeaponsSystem::getInstance().fireWeapon(entity->getWorld(), controller->pickedEntity, cameraForward,
                                                    viewMatrix, projectionMatrix);

        } else if (app->getKeyboard().justPressed(GLFW_KEY_R)) {
            WeaponsSystem::getInstance().reloadWeapon(controller->pickedEntity);
        }
    }

  public:
    // Enters the state and locks the mouse
    void enter(Application *app) {
        this->app = app;
        app->getMouse().lockMouse(app->getWindow());
        mouseLocked = true;
        moved = false;
        camera = nullptr;
        controller = nullptr;
    }

    float getSpeedMagnitude() { return glm::length(timeScaleVelocity); }

    bool isPlayerDead() { return controller && controller->isDead; }

    // Updates the FPS controller every frame
    void update(World *world, float deltaTime, float originalDeltaTime) {
        if (!(camera && controller)) {
            auto [camera, controller] = findControlledEntity(world);
            if (!(camera && controller))
                return;
            this->camera = camera;
            this->controller = controller;
        }

        if (isPlayerDead()) {
            controller->isSprinting = false;
            controller->isCrouching = false;
            controller->isJumping = false;
            glm::vec3 zeroVelocity = glm::vec3(0.0f);
            CollisionSystem::getInstance().moveGhost(camera->getOwner(), zeroVelocity, deltaTime);
            return;
        }
        auto characterController = controller->characterController.get();
        if (!characterController)
            return;

        Entity *entity = camera->getOwner();

        glm::vec3 &position = entity->localTransform.position * deltaTime;
        isGrounded = characterController->onGround();
        auto [viewMatrix, projectionMatrix] = getViewAndProjectionMatrix(entity);

        // Handle crouch toggling with C key
        handlePickup(controller, entity, viewMatrix, projectionMatrix);
        handleShooting(controller, entity, deltaTime, viewMatrix, projectionMatrix);

        handleMouseLockToggle();
        handleFov(camera, controller);
        handleStamina(controller, deltaTime);

        glm::vec3 movementDirection = handleMovement(controller, deltaTime);
        updateStillTimer(movementDirection, originalDeltaTime);
        applyMovementSmoothing(controller, movementDirection, deltaTime);
        handleJump(controller, deltaTime);

        glm::vec3 horizontalVelocity = glm::vec3(currentVelocity.x, 0.0f, currentVelocity.z);
        glm::vec3 totalMovement = horizontalVelocity * deltaTime;

        CollisionSystem::getInstance().moveGhost(entity, totalMovement, deltaTime);

        updateTimeScaleVelocity(controller, deltaTime);
        handleRotation(controller);

        handleCrouching(controller, entity, deltaTime);
        if (controller->bobEnabled)
            handleHeadBob(controller, entity, deltaTime, glm::length(movementDirection) > NEAR_ZERO);
    }

    float getTimeStandingStill() const { return timeStandingStill; }
    void turnOffCrosshair() { Crosshair::getInstance()->setWeaponHeld(false); }

    // Unlocks the mouse when the state exits
    void exit() {
        if (mouseLocked) {
            mouseLocked = false;
            app->getMouse().unlockMouse(app->getWindow());
            Crosshair::getInstance()->setWeaponHeld(false);
        }
    }
};
} // namespace our

