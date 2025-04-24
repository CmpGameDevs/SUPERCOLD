#pragma once
#include <algorithm>

namespace game {
    class TimeScaler {
        float timeScaleFactor;
        float minScale;
        float maxScale;

    public:
        TimeScaler()
            : timeScaleFactor(1.0f), minScale(0.1f), maxScale(4.0f) {}

        void update(float currentVelocity) {
            timeScaleFactor = std::clamp(currentVelocity / maxScale, minScale, maxScale);
        }

        float getTimeScale() const {
            return timeScaleFactor;
        }
    };
}