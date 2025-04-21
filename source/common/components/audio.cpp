#include "audio.hpp"
#include "../asset-loader.hpp"

namespace our
{
    AudioComponent::AudioComponent(size_t poolSize) {
        std::lock_guard<std::mutex> lock(audioMutex);
        sourcePool.resize(poolSize);
        for (auto &entry : sourcePool)
        {
            alGenSources(1, &entry.source);
            entry.inUse = false;
            entry.position = glm::vec3(0, 0, 0);
            entry.referenceDistance = 1.0f;
            entry.maxDistance = 100.0f;
            entry.rolloffFactor = 1.0f;
            entry.spatialized = false;
        }
    }

    AudioComponent::~AudioComponent() {
        stopAll();
        std::lock_guard<std::mutex> lock(audioMutex);
        for (auto &entry : sourcePool) {
            alDeleteSources(1, &entry.source);
        }
    }

    void AudioComponent::addSound(const std::string &name, AudioBuffer *buffer, std::string category) {
        if (soundBank.count(name)) return;
        std::lock_guard<std::mutex> lock(audioMutex);
        soundBank[name] = buffer;
        categories[name] = category;
    }

    void AudioComponent::configure3DSound(const std::string &soundName, float refDistance, float maxDistance, float rolloff) {
        std::lock_guard<std::mutex> lock(audioMutex);
        if (auto buf = soundBank.find(soundName); buf != soundBank.end()) {
            alSourcef(buf->second->bufferId, AL_REFERENCE_DISTANCE, refDistance);
            alSourcef(buf->second->bufferId, AL_MAX_DISTANCE, maxDistance);
            alSourcef(buf->second->bufferId, AL_ROLLOFF_FACTOR, rolloff);
        }
    }

    void AudioComponent::play(const std::string &soundName, const glm::vec3 &position, bool spatialized, bool loop, float volume) {
        std::lock_guard<std::mutex> lock(audioMutex);
        if (!soundBank.count(soundName)) return;

        for (auto &entry : sourcePool) {
            if (!entry.inUse) {
                ALint state;
                alGetSourcei(entry.source, AL_SOURCE_STATE, &state);

                if (state != AL_PLAYING) {
                    entry.inUse = true;
                    entry.position = position;
                    entry.spatialized = spatialized;

                    // Configure source
                    alSourcei(entry.source, AL_BUFFER, soundBank[soundName]->bufferId);
                    alSourcei(entry.source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
                    alSourcef(entry.source, AL_GAIN, volume);

                    if (spatialized) {
                        alSource3f(entry.source, AL_POSITION, position.x, position.y, position.z);
                        alSourcef(entry.source, AL_REFERENCE_DISTANCE, entry.referenceDistance);
                        alSourcef(entry.source, AL_MAX_DISTANCE, entry.maxDistance);
                        alSourcef(entry.source, AL_ROLLOFF_FACTOR, entry.rolloffFactor);
                    }

                    alSourcePlay(entry.source);

                    // Start monitoring thread
                    std::thread([this, source = entry.source, spatialized]() {
                        std::unique_lock<std::mutex> threadLock(audioMutex);
                        alcMakeContextCurrent(context);

                        ALint state;
                        do {
                            alGetSourcei(source, AL_SOURCE_STATE, &state);
                            threadLock.unlock();
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            threadLock.lock();
                        } while (state == AL_PLAYING);

                        for (auto &e : sourcePool) {
                            if (e.source == source) {
                                e.inUse = false;
                                break;
                            }
                        }
                        alcMakeContextCurrent(nullptr);
                    }).detach();

                    break;
                }
            }
        }
    }

    void AudioComponent::setSourcePosition(ALuint source, const glm::vec3 &position) {
        std::lock_guard<std::mutex> lock(audioMutex);
        alSource3f(source, AL_POSITION, position.x, position.y, position.z);
    }

    void AudioComponent::setSourceVelocity(ALuint source, const glm::vec3 &velocity) {
        std::lock_guard<std::mutex> lock(audioMutex);
        alSource3f(source, AL_VELOCITY, velocity.x, velocity.y, velocity.z);
    }

    void AudioComponent::updateMovingSource(ALuint source, const glm::vec3 &position, const glm::vec3 &velocity) {
        std::lock_guard<std::mutex> lock(audioMutex);
        alSource3f(source, AL_POSITION, position.x, position.y, position.z);
        alSource3f(source, AL_VELOCITY, velocity.x, velocity.y, velocity.z);
    }

    void AudioComponent::stop(const std::string &soundName) {
        std::lock_guard<std::mutex> lock(audioMutex);
        if (!soundBank.count(soundName)) return;

        for (auto &entry : sourcePool) {
            ALint buffer;
            alGetSourcei(entry.source, AL_BUFFER, &buffer);

            if (buffer == soundBank[soundName]->bufferId) {
                alSourceStop(entry.source);
                entry.inUse = false;
            }
        }
    }

    void AudioComponent::stopAll() {
        std::lock_guard<std::mutex> lock(audioMutex);
        for (auto &entry : sourcePool) {
            alSourceStop(entry.source);
            entry.inUse = false;
        }
    }

    void AudioComponent::setCategoryVolume(const std::string &category, float volume) {
        std::lock_guard<std::mutex> lock(audioMutex);
        for (auto &[sound, cat] : categories) {
            if (cat == category) setVolume(sound, volume);
        }
    }

    void AudioComponent::setVolume(const std::string &soundName, float volume) {
        std::lock_guard<std::mutex> lock(audioMutex);
        if (!soundBank.count(soundName)) return;

        for (auto &entry : sourcePool) {
            ALint buffer;
            alGetSourcei(entry.source, AL_BUFFER, &buffer);

            if (buffer == soundBank[soundName]->bufferId) {
                alSourcef(entry.source, AL_GAIN, volume);
            }
        }
    }

    void AudioComponent::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;
        
        // Read audio list from json
        if (data.contains("audio"))
        {
            for (auto &audio : data["audio"])
            {
                AudioBuffer *buffer = AssetLoader<AudioBuffer>::get(audio.get<std::string>());
                addSound(audio, buffer);
            }
        }
    }

}