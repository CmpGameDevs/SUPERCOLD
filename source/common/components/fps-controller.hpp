#pragma once

#include "../ecs/component.hpp"
#include <BulletDynamics/Character/btKinematicCharacterController.h>
#include <glm/glm.hpp>

namespace our {
class FPSControllerComponent : public Component {
  public:
    std::unique_ptr<btKinematicCharacterController> characterController = nullptr;
    float stepHeight = 0.35f;

    // Basic parameters
    float height = 1.8f; // Default height of the player
    float speed = 5.0f;  // Default speed of the player
    // Head bob parameters
    float headBobFrequency = 1.7f;     // How fast the head bobs
    float headBobAmplitude = 0.8f;     // How much the head bobs while walking
    float idleBobFrequency = 1.0f;     // Slower frequency for idle animation
    float idleBobAmplitude = 0.5f;     // Smaller amplitude for idle animation
    float sprintBobMultiplier = 1.02f; // Multiplier for head bob while sprinting
    float bobTimer = 0.0f;             // Timer for the head bob animation
    bool bobEnabled = false;

    // Basic movement parameters
    float pitch = 0.0f;
    float yaw = 0.0f;
    // Basic sensitivity settings
    float rotationSensitivityX = 0.01f;
    float rotationSensitivityY = 0.01f;
    bool invertYAxis = false;
    float fovSensitivity = 0.1f;
    float positionSensitivity = 0.1f;
    float speedupFactor = 2.0f;

    // Movement constraints
    float minVerticalRotation = -89.0f;
    float maxVerticalRotation = 89.0f;

    // Physics parameters
    float gravity = 9.8f;
    float jumpHeight = 1.0f;
    float jumpCooldown = 0.3f;

    // Input smoothing
    float movementSmoothing = 0.1f;
    float rotationSmoothing = 0.1f;
    float acceleration = 10.0f;
    float deceleration = 10.0f;

    // Advanced features
    float crouchHeightModifier = 0.5f;
    float crouchSpeedModifier = 0.6f;

    float actionSpeedModifier = 3.0f;
    float sprintSpeedModifier = 1.5f;
    float maxStamina = 100.0f;
    float staminaDepletionRate = 25.0f;
    float staminaRecoveryRate = 10.0f;
    float crouchTransitionTime = 0.2f;
    float crouchLerpProgress = 0.0f;
    float initialGhostHeight = 0.0f;
    float currentGhostHeight = height;
    float targetGhostHeight = 0.0f;

    // Current state variables
    bool isCrouching = false;
    bool isCrouchTransitioning = false;
    bool isSprinting = false;
    bool isJumping = false;
    float currentStamina = 100.0f;

    // Raycasting parameters
    float pickupDistance = 2.0f;
    float throwDistance = 5.0f;

    // Inventory parameters
    Entity *pickedEntity = nullptr;

    // Game Progression parameters
    bool isDead = false;

    ~FPSControllerComponent();

    // Deserialize from JSON
    void deserialize(const nlohmann::json &data);

    static std::string getID() { return "FPS Controller"; }
};
} // namespace our
