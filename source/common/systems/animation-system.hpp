#pragma once

#include "../ecs/world.hpp"

namespace our {

class AnimationSystem {
    public:
    AnimationSystem() = default;
    void update(World* world, float deltaTime);
};

}
