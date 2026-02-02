#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <cstdint>

struct AudioData {
    std::vector<double> samples;
    int channels;
    int sampleRate;
    int bitDepth;
};

inline AudioData LoadWav(const std::string &filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open wav file: " + filePath);
    }

    char chunkId[4];
    file.read(chunkId, 4);
    if (std::string(chunkId, 4) != "RIFF")
        throw std::runtime_error("Invalid RIFF header");

    file.seekg(4, std::ios::cur); // Skip size
    file.read(chunkId, 4);
    if (std::string(chunkId, 4) != "WAVE")
        throw std::runtime_error("Invalid WAVE header");

    AudioData data{};
    bool foundFmt = false;
    bool foundData = false;

    while (file.read(chunkId, 4)) {
        uint32_t chunkSize;
        file.read(reinterpret_cast<char *>(&chunkSize), 4);

        if (std::string(chunkId, 4) == "fmt ") {
            uint16_t format, channels, bitsPerSample;
            uint32_t sampleRate;
            file.read(reinterpret_cast<char *>(&format), 2);
            file.read(reinterpret_cast<char *>(&channels), 2);
            file.read(reinterpret_cast<char *>(&sampleRate), 4);
            file.seekg(6, std::ios::cur); // Skip byteRate and blockAlign
            file.read(reinterpret_cast<char *>(&bitsPerSample), 2);

            data.channels = channels;
            data.sampleRate = sampleRate;
            data.bitDepth = bitsPerSample;
            foundFmt = true;

            if (chunkSize > 16)
                file.seekg(chunkSize - 16, std::ios::cur);
        } else if (std::string(chunkId, 4) == "data") {
            if (!foundFmt)
                throw std::runtime_error("data chunk before fmt chunk");

            int numSamples = chunkSize / (data.bitDepth / 8);
            data.samples.resize(numSamples);

            if (data.bitDepth == 16) {
                std::vector<int16_t> temp(numSamples);
                file.read(reinterpret_cast<char *>(temp.data()), chunkSize);
                for (int i = 0; i < numSamples; ++i)
                    data.samples[i] = temp[i] / 32768.0;
            } else if (data.bitDepth == 24) {
                for (int i = 0; i < numSamples; ++i) {
                    uint8_t b[3];
                    file.read(reinterpret_cast<char *>(b), 3);
                    int32_t val = (b[0] << 8) | (b[1] << 16) | (b[2] << 24);
                    data.samples[i] = (val >> 8) / 8388608.0;
                }
            } else if (data.bitDepth == 32) {
                std::vector<float> temp(numSamples);
                file.read(reinterpret_cast<char *>(temp.data()), chunkSize);
                for (int i = 0; i < numSamples; ++i)
                    data.samples[i] = static_cast<double>(temp[i]);
            }
            foundData = true;
            break;
        } else {
            file.seekg(chunkSize, std::ios::cur);
        }
    }

    if (!foundData)
        throw std::runtime_error("data chunk not found");
    return data;
}
