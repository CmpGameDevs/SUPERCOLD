#pragma once
#include <ecs/component.hpp>
#include <ecs/entity.hpp>
#include <ecs/world.hpp>
#include <unordered_map>

namespace our {

class WeaponsSystem {
    WeaponsSystem() = default;
    WeaponsSystem(const WeaponsSystem&) = delete; // Prevent copying
    WeaponsSystem& operator=(const WeaponsSystem&) = delete; // Prevent assignment
    WeaponsSystem(WeaponsSystem&&) = delete; // Prevent moving
    WeaponsSystem& operator=(WeaponsSystem&&) = delete; // Prevent moving assignment

    std::unordered_map<Entity*, Component*> weaponsMap;

    void _removeCollisionComponent(Entity* entity);

    Component *_addCollisionComponent(Entity* entity);

public:
    static WeaponsSystem& getInstance() {
        static WeaponsSystem instance;
        return instance;
    }

    void update(World* world, float deltaTime);

    void throwWeapon(World* world, Entity* entity, glm::vec3 forward);

    void dropWeapon(World* world, Entity* entity);

    void reloadWeapon(World* world, Entity* entity);

    void fireWeapon(World* world, Entity* entity, glm::vec3 direction);

    void pickupWeapon(World* world, Entity* entity, Entity* weaponEntity);
};

}