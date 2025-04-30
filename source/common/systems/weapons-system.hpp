#pragma once
#include <ecs/component.hpp>
#include <ecs/entity.hpp>
#include <ecs/world.hpp>
#include <unordered_map>
#include <unordered_set>

namespace our {

class WeaponsSystem {
    WeaponsSystem() = default;
    WeaponsSystem(const WeaponsSystem&) = delete; // Prevent copying
    WeaponsSystem& operator=(const WeaponsSystem&) = delete; // Prevent assignment
    WeaponsSystem(WeaponsSystem&&) = delete; // Prevent moving
    WeaponsSystem& operator=(WeaponsSystem&&) = delete; // Prevent moving assignment

    std::unordered_map<Entity*, Component*> weaponsMap;

    struct Projectile {
        Entity* entity;
        Entity* owner;
        glm::vec3 direction;
        float speed;
        float range;
        float lifetime;
        float timeAlive = 0.0f;

        Projectile(Entity* entity, Entity* owner, glm::vec3 direction, float speed, float range, float lifetime)
            : entity(entity), owner(owner), direction(direction), speed(speed), range(range), lifetime(lifetime) {}

        ~Projectile() {
            owner = nullptr;
        }

        bool operator==(const Projectile& other) const {
            return entity == other.entity;
        }
    };

    std::unordered_set<Projectile*> projectiles;

    void _removeCollisionComponent(Entity* entity);

    Component *_addCollisionComponent(Entity* entity);

    Entity *_createProjectile(World* world, Entity* owner, glm::vec3 direction, float speed);

public:
    static WeaponsSystem& getInstance() {
        static WeaponsSystem instance;
        return instance;
    }

    void update(World* world, float deltaTime);

    bool throwWeapon(World* world, Entity* entity, glm::vec3 forward);

    bool dropWeapon(World* world, Entity* entity);

    bool reloadWeapon(World* world, Entity* entity);

    bool fireWeapon(World* world, Entity* entity, glm::vec3 direction);

    bool pickupWeapon(World* world, Entity* entity, Entity* weaponEntity);

    void onDestroy();
};

}