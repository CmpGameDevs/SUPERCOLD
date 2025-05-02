#pragma once
#include <AL/al.h>
#include <AL/alc.h>
#include <audio/audio-buffer.hpp>
#include <ecs/component.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace our {

class AudioComponent : public Component {
  private:
    struct ActiveSound {
        ALuint source;
        bool inUse;
        glm::vec3 position;
        float referenceDistance;
        float maxDistance;
        float rolloffFactor;
        bool spatialized;
    };

    static inline std::mutex audioMutex;
    static inline ALCcontext *context = nullptr;

    std::vector<ActiveSound> sourcePool;
    std::unordered_map<std::string, AudioBuffer *> soundBank;
    std::unordered_map<std::string, std::string> categories;

  public:
    static std::string getID() { return "Audio"; }

    static void setContext(ALCcontext *ctx) {
        std::lock_guard<std::mutex> lock(audioMutex);
        context = ctx;
    }

    explicit AudioComponent(size_t poolSize = 8);

    ~AudioComponent();

    std::vector<ActiveSound>& getSourcePool() { return sourcePool; }

    void deserialize(const nlohmann::json& data) override;

    void addSound(const std::string& name, AudioBuffer* buffer, std::string category = "sfx");

    void configure3DSound(const std::string &soundName, float refDistance = 1.0f, float maxDistance = 100.0f,
                          float rolloff = 1.0f);

    void play(const std::string &soundName, const glm::vec3 &position = glm::vec3(0), bool spatialized = false,
              bool loop = false, float volume = 1.0f);

    void setSourcePosition(ALuint source, const glm::vec3 &position);

    void setSourceVelocity(ALuint source, const glm::vec3 &velocity);

    void updateMovingSource(ALuint source, const glm::vec3 &position, const glm::vec3 &velocity);

    void pollFinishedSources();

    void stop(const std::string &soundName);

    void stopAll();

    void setCategoryVolume(const std::string &category, float volume);

    void setVolume(const std::string &soundName, float volume);
};

} // namespace our