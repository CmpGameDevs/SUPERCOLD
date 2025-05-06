#pragma once
#include <algorithm>

namespace game {
    class TimeScaler {
        float timeScaleFactor;
        float minScale;
        float maxScale;

        float playerTimeScaleFactor;
        float playerMinScale;
        float playerMaxScale;

    public:
        TimeScaler()
            : playerTimeScaleFactor(1.0f), timeScaleFactor(1.0f), minScale(0.1f), maxScale(4.0f), playerMinScale(0.0f), playerMaxScale(0.6f) {}

        void updateWorldTimeScale(float currentVelocity) {
            timeScaleFactor = std::clamp(currentVelocity / maxScale, minScale, 1.0f);
        }

        float getWorldTimeScale() const {
            return timeScaleFactor;
        }

        void updatePlayerTimeScale(float vignetteIntensity) {
            if(vignetteIntensity < playerMinScale)
                playerTimeScaleFactor = 1.0f - playerMinScale;
            else
                playerTimeScaleFactor = 1.0f - std::clamp(vignetteIntensity / playerMaxScale, playerMinScale, 1.0f);
        }

        float getPlayerTimeScale() const {
            return playerTimeScaleFactor;
        }
    };
}