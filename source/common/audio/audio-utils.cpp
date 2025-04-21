#include "audio-utils.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

struct RiffHeader {
    char chunkID[4];     // "RIFF"
    uint32_t chunkSize;
    char format[4];      // "WAVE"
};

struct FmtSubchunk {
    char subchunkID[4];  // "fmt "
    uint32_t subchunkSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};

struct DataSubchunk {
    char subchunkID[4];  // "data"
    uint32_t subchunkSize;
};

namespace our::audio_utils {
    AudioBuffer* loadWavFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to open file: " + filename);
        }

        // Read main header
        RiffHeader riffHeader;
        file.read(reinterpret_cast<char*>(&riffHeader), sizeof(RiffHeader));
        
        if (std::string(riffHeader.chunkID, 4) != "RIFF" ||
            std::string(riffHeader.format, 4) != "WAVE") {
            throw std::runtime_error("Invalid RIFF/WAVE header in: " + filename);
        }

        // Read fmt subchunk
        FmtSubchunk fmtSubchunk;
        file.read(reinterpret_cast<char*>(&fmtSubchunk), sizeof(FmtSubchunk));
        
        if (std::string(fmtSubchunk.subchunkID, 4) != "fmt ") {
            throw std::runtime_error("fmt subchunk not found in: " + filename);
        }

        if (fmtSubchunk.audioFormat != 1 || fmtSubchunk.bitsPerSample != 16) {
            throw std::runtime_error("Only 16-bit PCM supported in: " + filename);
        }

        // Skip any extra format bytes
        if (fmtSubchunk.subchunkSize > 16) {
            file.seekg(fmtSubchunk.subchunkSize - 16, std::ios::cur);
        }

        // Find data subchunk
        DataSubchunk dataSubchunk;
        while(true) {
            file.read(reinterpret_cast<char*>(&dataSubchunk), sizeof(DataSubchunk));
            if (std::string(dataSubchunk.subchunkID, 4) == "data") break;
            
            // Skip unknown chunks
            file.seekg(dataSubchunk.subchunkSize, std::ios::cur);
        }

        // Read audio data
        std::vector<char> audioData(dataSubchunk.subchunkSize);
        if (!file.read(audioData.data(), dataSubchunk.subchunkSize)) {
            throw std::runtime_error("Failed to read audio data in: " + filename);
        }

        // Create OpenAL buffer
        ALuint buffer;
        alGenBuffers(1, &buffer);
        
        ALenum format = (fmtSubchunk.numChannels == 1) ? 
            AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

        alBufferData(buffer, format, audioData.data(), 
                    static_cast<ALsizei>(audioData.size()), 
                    fmtSubchunk.sampleRate);

        return new AudioBuffer(buffer, format, fmtSubchunk.sampleRate);
    }
}