#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include "../animation/animation-player.hpp"
#include "../ecs/component.hpp"
#include "../model/model.hpp"

namespace our {

class AnimationComponent : public Component {
    public:
    Model* modelAsset = nullptr;

    AnimationPlayer player;

    std::string initialAnimationName = "";
    bool autoPlay = true;
    bool initialized = false;

    static std::string getID() {
        return "Animation";
    }

    AnimationComponent() = default;

    void initialize();

    void update(float deltaTime);

    bool playAnimation(const std::string& animationName, bool loop = true);

    void pauseAnimation();
    void resumeAnimation();
    void stopAnimation();

    void deserialize(const nlohmann::json& data) override;
};

} // namespace our
