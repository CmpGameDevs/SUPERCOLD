#include "audio-system.hpp"
#include <asset-loader.hpp>

namespace our {

    void AudioSystem::initialize(ALCcontext* context) {
        our::AudioComponent::setContext(context);
        alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
        initializeCategories();

        const size_t globalPoolSize = 8;
        globalSfxHandler = std::make_unique<AudioComponent>(globalPoolSize);
    }

    void AudioSystem::initializeCategories() {
        categories["sfx"] = {sfxVolume};
        categories["music"] = {musicVolume};
        categories["voice"] = {1.0f};
        categories["ambient"] = {0.7f};
    }

    void AudioSystem::update(World* world, float deltaTime) {
        std::lock_guard<std::mutex> lock(audioMutex);
        _updateListener();
        _updateBackgroundMusic(deltaTime);

        if (globalSfxHandler) {
            globalSfxHandler->pollFinishedSources();
        }

        if (!world) return;
        for(auto entity : world->getEntities()){
            AudioComponent* audio = entity->getComponent<AudioComponent>();
            if(audio){
                audio->pollFinishedSources();
                _updateComponent(entity, audio, deltaTime);
            }
        }
        _updateCategoryVolumes(world, deltaTime);
    }

    void AudioSystem::playSfx(const std::string& name, bool loop, float volume) {
        const std::string category = "sfx";
        auto& cat = categories[category];
        std::lock_guard<std::mutex> lock(audioMutex);
        AudioBuffer* buffer = AssetLoader<AudioBuffer>::get(name);
        globalSfxHandler->addSound(name, buffer, category);
        globalSfxHandler->play(name, glm::vec3(0), false, loop, volume * cat.volume);
    }

    void AudioSystem::stopSfx(const std::string& name) {
        std::lock_guard<std::mutex> lock(audioMutex);
        globalSfxHandler->stop(name);
    }

    void AudioSystem::playSpatialSound(const std::string& name,
                                      Entity* entity, 
                                      const glm::vec3& position,
                                      const std::string& category,
                                      bool loop,
                                      float referenceDistance,
                                      float maxDistance) {
        if (!entity) return;
        std::lock_guard<std::mutex> lock(audioMutex);
        
        auto& cat = categories[category];
        AudioBuffer* buffer = AssetLoader<AudioBuffer>::get(name);
        AudioComponent* audio = entity->getComponent<AudioComponent>();
        if (!audio) audio = entity->addComponent<AudioComponent>();
        audio->addSound(name, buffer, category);
        audio->play(name, position, true, loop, cat.volume);
        audio->configure3DSound(name, referenceDistance, maxDistance);
    }

    void AudioSystem::playBackgroundMusic(const std::string& name,
                                         float fadeDuration,
                                         const std::string& category) {
        std::lock_guard<std::mutex> lock(audioMutex);
        if (backgroundTrack.currentTrack == name) return;

        if(backgroundTrack.component) {
            categories[category].targetVolume = 0.0f;
            categories[category].fadeSpeed = 1.0f / fadeDuration;
        }

        AudioBuffer* buffer = AssetLoader<AudioBuffer>::get(name);
        if (!buffer) return;

        backgroundTrack.component = std::make_unique<AudioComponent>();
        backgroundTrack.component->addSound(name, buffer, category);
        backgroundTrack.currentTrack = name;
        auto& cat = categories[category];
        
        cat.targetVolume = cat.volume;
        cat.volume = 0.0f;
        cat.fadeSpeed = 1.0f / fadeDuration;
        
        backgroundTrack.component->play(name, glm::vec3(0), false, true, cat.volume);
    }

    void AudioSystem::setListenerPosition(const glm::vec3& position, 
                                         const glm::vec3& velocity,
                                         const glm::vec3& forward,
                                         const glm::vec3& up) {
        listenerPosition = position;
        listenerVelocity = velocity;
        listenerForward = forward;
        listenerUp = up;
    }

    void AudioSystem::setListenerGain(float gain) {
        std::lock_guard<std::mutex> lock(audioMutex);
        alListenerf(AL_GAIN, gain);
    }

    void AudioSystem::setCategoryVolume(const std::string& category, float volume, float fadeTime) {
        auto& cat = categories[category];
        if(fadeTime <= 0.0f) {
            cat.volume = volume;
            cat.targetVolume = volume;
        } else {
            cat.targetVolume = volume;
            cat.fadeSpeed = 1.0f / fadeTime;
        }
    }

    void AudioSystem::_updateComponent(Entity* entity, AudioComponent* audio, float deltaTime) {
        glm::vec3 position = entity->localTransform.position;
        for(auto& source : audio->getSourcePool()) {
            if(source.spatialized) {
                source.position = position;
                audio->setSourcePosition(source.source, source.position);
                audio->setSourceVelocity(source.source, listenerVelocity);
            }
        }
    }

    void AudioSystem::_updateListener() {
        alListener3f(AL_POSITION, 
                    listenerPosition.x, 
                    listenerPosition.y, 
                    listenerPosition.z);
        
        alListener3f(AL_VELOCITY, 
                    listenerVelocity.x, 
                    listenerVelocity.y, 
                    listenerVelocity.z);
        
        float orientation[6] = {
            listenerForward.x, listenerForward.y, listenerForward.z,
            listenerUp.x, listenerUp.y, listenerUp.z
        };
        alListenerfv(AL_ORIENTATION, orientation);
    }

    void AudioSystem::_updateBackgroundMusic(float deltaTime) {
        if(!backgroundTrack.component) return;
        auto& cat = categories["music"];
        cat.volume = glm::mix(cat.volume, cat.targetVolume, deltaTime * cat.fadeSpeed);
        backgroundTrack.component->setVolume(backgroundTrack.currentTrack, cat.volume);
        if(cat.volume <= 0.01f && cat.targetVolume <= 0.0f) {
            backgroundTrack.component->stopAll();
            backgroundTrack.component = nullptr;
        }
    }

    void AudioSystem::_updateCategoryVolumes(World* world, float deltaTime) {
        for (auto& entity : world->getEntities()) {
            AudioComponent* audio = entity->getComponent<AudioComponent>();
            if (!audio) continue;
            for (auto& [name, category] : categories) {
                if (category.volume != category.targetVolume) {
                    category.volume = glm::mix(category.volume, 
                                             category.targetVolume, 
                                             deltaTime * category.fadeSpeed);
                    
                    // Update all sounds in this category
                    audio->setVolume(name, category.volume);
                }
            }
        }
    }
}