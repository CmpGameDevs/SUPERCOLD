#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <ecs/component.hpp>

namespace our {

    class TrailRenderer : public Component {
    public:
        std::vector<glm::vec3> trailPoints;
        size_t maxTrailPoints = 20;
        float pointLifetime = 0.5f;
        float timeSinceLastPoint = 0.0f;
        float pointAddInterval = 0.02f;
        glm::vec4 trailColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);   // Red as in SUPERHOT
        float startWidth = 0.05f;
        float endWidth = 0.02f;

        static std::string getID() { return "TrailRenderer"; }
        void deserialize(const nlohmann::json& data) override;
    };
}