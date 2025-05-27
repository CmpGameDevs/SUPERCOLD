#include "animation-component.hpp"
#include <iostream>
#include "../asset-loader.hpp"

namespace our {

void AnimationComponent::initialize() {
    if (!modelAsset) {
        std::cerr << "[AnimationComponent] ERROR: modelAsset is null. Cannot initialize animations." << std::endl;
        return;
    }
    if (autoPlay && !initialAnimationName.empty()) {
        playAnimation(initialAnimationName, true);
    }
}

void AnimationComponent::update(float deltaTime) {
    if (modelAsset) {
        player.update(deltaTime);
    }
}

bool AnimationComponent::playAnimation(const std::string& animationName, bool loop) {
    if (!modelAsset) {
        std::cerr << "[AnimationComponent] Cannot play animation '" << animationName << "': modelAsset is null."
                  << std::endl;
        return false;
    }

    Animation* animToPlay = nullptr;
    for (auto& anim : modelAsset->animations) {
        if (anim.name == animationName) {
            animToPlay = &anim;
            break;
        }
    }

    if (animToPlay) {
        player.playAnimation(animToPlay, loop);
        std::cout << "[AnimationComponent] Started animation: '" << animationName << "'" << std::endl;
        return true;
    } else {
        std::cerr << "[AnimationComponent] Animation '" << animationName << "' not found in model asset." << std::endl;
        return false;
    }
}

void AnimationComponent::pauseAnimation() {
    player.pause();
}

void AnimationComponent::resumeAnimation() {
    player.resume();
}

void AnimationComponent::stopAnimation() {
    player.stop();
}

void AnimationComponent::deserialize(const nlohmann::json& data) {
    if (!data.is_object())
        return;

    initialAnimationName = data.value("initialAnimation", initialAnimationName);
    autoPlay = data.value("autoPlay", autoPlay);

    std::string modelName = data.value("model", "");
    if (!modelName.empty()) {
        modelAsset = AssetLoader<Model>::get(modelName);
        if (!modelAsset) {
            std::cerr << "[AnimationComponent] ERROR: Failed to find model asset '" << modelName
                      << "' during deserialization." << std::endl;
        }
    }
}

} // namespace our
