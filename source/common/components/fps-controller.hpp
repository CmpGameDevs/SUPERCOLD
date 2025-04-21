#pragma once

#include "../ecs/component.hpp"

#include <glm/glm.hpp>

namespace our {
class FPSControllerComponent : public Component {
  public:
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

    float sprintSpeedModifier = 1.5f;
    float maxStamina = 100.0f;
    float staminaDepletionRate = 25.0f;
    float staminaRecoveryRate = 10.0f;

    // Current state variables
    bool isCrouching = false;
    bool isSprinting = false;
    bool isJumping = false;
    float currentStamina = 100.0f;

    // Deserialize from JSON
    void deserialize(const nlohmann::json &data);

    static std::string getID() { return "FPS Controller"; }
};
} // namespace our
