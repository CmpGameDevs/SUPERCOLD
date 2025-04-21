#pragma once

#include <AL/al.h>
#include <AL/alc.h>

namespace our {
    class AudioBuffer {
    public:
        ALuint bufferId;
        ALenum format;
        ALsizei sampleRate;
    
        AudioBuffer(ALuint buffer, ALenum fmt, ALsizei rate)
            : bufferId(buffer), format(fmt), sampleRate(rate) {}
        
        ~AudioBuffer() {
            alDeleteBuffers(1, &bufferId);
        }
    };
}