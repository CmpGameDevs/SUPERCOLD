#pragma once
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <AL/al.h>
#include <AL/alc.h>
#include <glm/glm.hpp>
#include <ecs/world.hpp>
#include <audio/audio-buffer.hpp>
#include <components/audio.hpp>

namespace our {

class AudioSystem {
private:
    struct AudioCategory {
        float volume = 1.0f;
        float targetVolume = 1.0f;
        float fadeSpeed = 1.0f;
    };

    struct BackgroundTrack {
        std::unique_ptr<AudioComponent> component = nullptr;
        std::string currentTrack;
        float fadeProgress = 0.0f;
    };

    glm::vec3 listenerPosition;
    glm::vec3 listenerVelocity;
    glm::vec3 listenerForward = glm::vec3(0, 0, -1);
    glm::vec3 listenerUp = glm::vec3(0, 1, 0);
    
    std::unordered_map<std::string, AudioCategory> categories;
    BackgroundTrack backgroundTrack;

    std::mutex audioMutex;

    std::unique_ptr<AudioComponent> globalSfxHandler = nullptr;

public:
    float sfxVolume = 1.0f;
    float musicVolume = 0.2f;

    static AudioSystem& getInstance() {
        static AudioSystem instance;
        return instance;
    }

    void initialize(ALCcontext* context);

    void initializeCategories();

    void update(World* world, float deltaTime);

    void stopSfx(const std::string& name);

    void playSfx(const std::string& name,
                bool loop = false,
                float volume = 1.0f);

    void playSpatialSound(const std::string& name, 
                         Entity* entity,
                         const glm::vec3& position,
                         const std::string& category = "sfx",
                         bool loop = false,
                         float referenceDistance = 1.0f,
                         float maxDistance = 100.0f);

    void playBackgroundMusic(const std::string& name,
                            float fadeDuration = 1.0f,
                            const std::string& category = "music");

    void setListenerPosition(const glm::vec3& position, 
                            const glm::vec3& velocity = glm::vec3(0),
                            const glm::vec3& forward = glm::vec3(0,0,-1),
                            const glm::vec3& up = glm::vec3(0,1,0));

    void setListenerGain(float gain);

    void setCategoryVolume(const std::string& category, float volume, float fadeTime = 0.0f);

private:
    AudioSystem() = default;

    void _updateComponent(Entity* entity, AudioComponent* audio, float deltaTime);

    void _updateListener();

    void _updateBackgroundMusic(float deltaTime);

    void _updateCategoryVolumes(World* world, float deltaTime);
};

}