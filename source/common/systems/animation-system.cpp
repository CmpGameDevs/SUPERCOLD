#include "animation-system.hpp"
#include "../components/animation-component.hpp"

#include <glm/glm.hpp>
#include <iostream>

namespace our {

void AnimationSystem::update(World* world, float deltaTime) {
    if (!world)
        return;

    for (auto entity : world->getEntities()) {
        AnimationComponent* animComp = entity->getComponent<AnimationComponent>();

        if (!animComp || !animComp->modelAsset || !animComp->modelAsset->skeleton.getBoneCount()) {

            if (animComp) {
                std::cerr << "[AnimationSystem] WARNING: Entity '" << entity->name
                          << "' has an AnimationComponent but no valid modelAsset or skeleton." << std::endl;
            }
            continue;
        }

        animComp->update(deltaTime);

        Skeleton* skeleton = &(animComp->modelAsset->skeleton);
        AnimationPlayer* player = &(animComp->player);

        glm::mat4 skeletonRootNodeTransform = glm::mat4(1.0f);
        skeleton->calculateAnimatedPose(player, skeletonRootNodeTransform);
    }
}

} // namespace our
