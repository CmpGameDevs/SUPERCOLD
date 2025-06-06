#pragma once

#include "../ecs/entity.hpp"
#include "camera.hpp"
#include "components/animation-component.hpp"
#include "fps-controller.hpp"
#include "free-camera-controller.hpp"
#include "mesh-renderer.hpp"
#include "movement.hpp"
#include "collision.hpp"
#include "model-renderer.hpp"
#include "audio.hpp"
#include "weapon.hpp"
#include "enemy-controller.hpp"

namespace our {

// Given a json object, this function picks and creates a component in the given entity
// based on the "type" specified in the json object which is later deserialized from the rest of the json object
inline void deserializeComponent(const nlohmann::json &data, Entity *entity) {
    std::string type = data.value("type", "");
    Component *component = nullptr;
    // TODO: (Req 8) Add an option to deserialize a "MeshRendererComponent" to the following if-else statement
    if (type == CameraComponent::getID()) {
        component = entity->addComponent<CameraComponent>();
    } else if (type == FreeCameraControllerComponent::getID()) {
        component = entity->addComponent<FreeCameraControllerComponent>();
    } else if (type == FPSControllerComponent::getID()) {
        component = entity->addComponent<FPSControllerComponent>();
    } else if (type == MovementComponent::getID()) {
        component = entity->addComponent<MovementComponent>();
    } else if (type == MeshRendererComponent::getID()) {
        component = entity->addComponent<MeshRendererComponent>();
    } else if (type == CollisionComponent::getID()) {
        component = entity->addComponent<CollisionComponent>();
    } else if (type == ModelComponent::getID()) {
        component = entity->addComponent<ModelComponent>();
    } else if (type == AudioComponent::getID()) {
        component = entity->addComponent<AudioComponent>();
    } else if (type == WeaponComponent::getID()) {
        component = entity->addComponent<WeaponComponent>();
    } else if (type == EnemyControllerComponent::getID()) {
        component = entity->addComponent<EnemyControllerComponent>();
    } else if (type == AnimationComponent::getID()) {
        component = entity->addComponent<AnimationComponent>();
    } else {
        std::cerr << "[Deserialize] Unknown component type: " << type << std::endl;
        return;
    }

    if (component)
        component->deserialize(data);
}

}
