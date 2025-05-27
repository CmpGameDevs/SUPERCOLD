#include "trail-renderer.hpp"
#include <ecs/entity.hpp>
#include <deserialize-utils.hpp>

namespace our {

    void TrailRenderer::deserialize(const nlohmann::json &data) {
        if (data.contains("maxTrailPoints")) maxTrailPoints = data["maxTrailPoints"];
        if (data.contains("pointLifetime")) pointLifetime = data["pointLifetime"];
        if (data.contains("pointAddInterval")) pointAddInterval = data["pointAddInterval"];
        if (data.contains("startWidth")) startWidth = data["startWidth"];
        if (data.contains("endWidth")) endWidth = data["endWidth"];
        if (data.contains("color")) trailColor = data["color"];
    }

}