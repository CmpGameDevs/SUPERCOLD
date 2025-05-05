#include "fps-controller.hpp"
#include <systems/collision-system.hpp>


namespace our {

FPSControllerComponent::~FPSControllerComponent() {
    if (characterController) {
        CollisionSystem::getInstance().getPhysicsWorld()->removeAction(characterController.get());
        characterController.reset();
    }
}
    
// Reads configuration from the given json object
void FPSControllerComponent::deserialize(const nlohmann::json &data) {
    if (!data.is_object())
        return;

    // Basic sensitivity settings
    rotationSensitivityX = data.value("rotationSensitivityX", rotationSensitivityX);
    rotationSensitivityY = data.value("rotationSensitivityY", rotationSensitivityY);
    invertYAxis = data.value("invertYAxis", invertYAxis);
    fovSensitivity = data.value("fovSensitivity", fovSensitivity);
    positionSensitivity = data.value("positionSensitivity", positionSensitivity);
    speedupFactor = data.value("speedupFactor", speedupFactor);

    // Movement constraints
    minVerticalRotation = data.value("minVerticalRotation", minVerticalRotation);
    maxVerticalRotation = data.value("maxVerticalRotation", maxVerticalRotation);

    // Physics parameters
    gravity = data.value("gravity", gravity);
    jumpHeight = data.value("jumpHeight", jumpHeight);
    jumpCooldown = data.value("jumpCooldown", jumpCooldown);

    // Input smoothing
    movementSmoothing = data.value("movementSmoothing", movementSmoothing);
    rotationSmoothing = data.value("rotationSmoothing", rotationSmoothing);
    acceleration = data.value("acceleration", acceleration);
    deceleration = data.value("deceleration", deceleration);

    // Advanced features
    crouchHeightModifier = data.value("crouchHeightModifier", crouchHeightModifier);
    crouchSpeedModifier = data.value("crouchSpeedModifier", crouchSpeedModifier);

    sprintSpeedModifier = data.value("sprintSpeedModifier", sprintSpeedModifier);
    maxStamina = data.value("maxStamina", maxStamina);
    staminaDepletionRate = data.value("staminaDepletionRate", staminaDepletionRate);
    staminaRecoveryRate = data.value("staminaRecoveryRate", staminaRecoveryRate);

    // Initialize current state
    currentStamina = maxStamina;
}

} // namespace our
