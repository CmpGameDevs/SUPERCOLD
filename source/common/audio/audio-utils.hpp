#pragma once
#include <AL/al.h>
#include <AL/alc.h>
#include <string>
#include <memory>
#include "audio-buffer.hpp"

namespace our::audio_utils {
    AudioBuffer* loadWavFile(const std::string& filename);
}