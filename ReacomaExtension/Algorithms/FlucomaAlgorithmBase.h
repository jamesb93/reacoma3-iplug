#pragma once

#include "IAlgorithm.h"
#include "ReacomaExtension.h"
#include "reaper_plugin_functions.h"
#include "../VectorBufferAdaptor.h"
#include "../../dependencies/flucoma-core/include/flucoma/clients/common/FluidContext.hpp"
#include "../../dependencies/flucoma-core/include/flucoma/clients/common/Result.hpp"
#include <iomanip>
#include <filesystem>

using namespace fluid;
using namespace client;

// Forward declarations
class MediaItem;
class MediaItem_Take;

template <typename ClientType>
class FlucomaAlgorithm : public IAlgorithm {
public:
    FlucomaAlgorithm(ReacomaExtension* apiProvider)
        : IAlgorithm(apiProvider),
          mContext{},
          mParams{ClientType::getParameterDescriptors(), FluidDefaultAllocator()},
          mClient{mParams, mContext}
    {
        mClient.setSynchronous(true); // Ideal for REAPER integration
    }

    virtual ~FlucomaAlgorithm() override = default;

    // The "Template Method"
    bool ProcessItem(MediaItem* item) override final {
        if (!item || !mApiProvider) return false;

        MediaItem_Take* take = GetActiveTake(item);
        if (!take) return false;

        PCM_source* source = GetMediaItemTake_Source(take);
        if (!source) return false;

        const int sampleRate = GetMediaSourceSampleRate(source);
        const int numChannels = GetMediaSourceNumChannels(source);
        const double itemLength = GetMediaItemInfo_Value(item, "D_LENGTH");
        const double playrate = GetMediaItemTakeInfo_Value(take, "D_PLAYRATE");
        const double takeOffset = GetMediaItemTakeInfo_Value(take, "D_STARTOFFS");
        const double sourceDuration = source->GetLength();

        const double effectiveTakeDuration = itemLength * playrate;
        const double actualDurationToProcess = std::min(effectiveTakeDuration, sourceDuration - takeOffset);
        const int frameCount = static_cast<int>(sampleRate * actualDurationToProcess);

        if (frameCount <= 0 || numChannels <= 0) return false;
        
        // Prepare audio buffer
        std::vector<double> allChannelsAsDouble(frameCount * numChannels);
        PCM_source_transfer_t transfer{};
        transfer.time_s = takeOffset;
        transfer.samplerate = static_cast<double>(sampleRate);
        transfer.nch = numChannels;
        transfer.length = frameCount;
        transfer.samples = allChannelsAsDouble.data();
        source->GetSamples(&transfer);

        std::vector<float> allChannelsAsFloat(allChannelsAsDouble.begin(), allChannelsAsDouble.end());
        auto inputBuffer = InputBufferT::type(new fluid::VectorBufferAdaptor(allChannelsAsFloat, numChannels, frameCount, sampleRate));

        // Defer algorithm-specific processing to the derived class
        if (!DoProcess(inputBuffer, numChannels, frameCount, sampleRate)) {
            return false;
        }
        
        // Defer result handling to the derived class
        if (!HandleResults(item, take, numChannels, sampleRate)) {
            return false;
        }

        UpdateTimeline();
        return true;
    }

protected:
    /**
     * @brief Performs the core algorithm processing.
     *
     * This pure virtual function must be implemented by derived classes. It is responsible
     * for setting up the algorithm's parameters in `mParams` and executing the process.
     *
     * @param sourceBuffer The input audio buffer.
     * @param numChannels The number of channels in the source audio.
     * @param frameCount The number of frames in the source audio.
     * @param sampleRate The sample rate of the source audio.
     * @return true if processing was successful, false otherwise.
     */
    virtual bool DoProcess(InputBufferT::type& sourceBuffer, int numChannels, int frameCount, int sampleRate) = 0;

    /**
     * @brief Handles the output of the algorithm.
     *
     * This pure virtual function must be implemented by derived classes. It is responsible
     * for taking the results from the processed buffers in `mParams` and applying them
     * to the REAPER project (e.g., creating new takes, adding markers).
     *
     * @param item The original MediaItem.
     * @param take The active take of the MediaItem.
     * @param numChannels The number of channels in the original audio.
     * @param sampleRate The sample rate of the original audio.
     * @return true if handling was successful, false otherwise.
     */
    virtual bool HandleResults(MediaItem* item, MediaItem_Take* take, int numChannels, int sampleRate) = 0;

protected:
    FluidContext mContext;
    typename ClientType::ParamSetType mParams;
    ClientType mClient;
};

/**
 * @class AudioOutputAlgorithm
 * @brief An intermediate base class for algorithms that produce new audio files.
 *
 * This class provides a concrete implementation of `AddOutputToTake`, which is
 * shared by algorithms like NMF and HPSS.
 */
template <typename ClientType>
class AudioOutputAlgorithm : public FlucomaAlgorithm<ClientType> {
protected:
    using FlucomaAlgorithm<ClientType>::mApiProvider; // Make base members visible
    
    AudioOutputAlgorithm(ReacomaExtension* apiProvider)
        : FlucomaAlgorithm<ClientType>(apiProvider) {}

    void AddOutputToTake(MediaItem* item, BufferT::type output, int sampleRate, const std::string& suffix) {
        if (!output) return;
        
        fluid::client::BufferAdaptor::ReadAccess bufferReader(output.get());
        if (!bufferReader.exists() || !bufferReader.valid()) return;

        auto numFrames = bufferReader.numFrames();
        auto numChans = bufferReader.numChans();

        std::vector<std::vector<ReaSample>> channelData(numChans, std::vector<ReaSample>(numFrames));
        std::vector<ReaSample*> pointerArray(numChans);
        const float* sourceData = bufferReader.allFrames().data();

        for (int i = 0; i < numChans; ++i) {
            for (int j = 0; j < numFrames; ++j) {
                channelData[i][j] = static_cast<ReaSample>(sourceData[j * numChans + i]);
            }
            pointerArray[i] = channelData[i].data();
        }

        char originalFilePathCStr[4096] = "";
        auto activeTake = GetActiveTake(item);
        if (activeTake) {
            auto takeSource = GetMediaItemTake_Source(activeTake);
            if (takeSource) {
                auto srcParent = GetMediaSourceParent(takeSource);
                GetMediaSourceFileName(srcParent ? srcParent : takeSource, originalFilePathCStr, sizeof(originalFilePathCStr));
            }
        }
        
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d%H%M%S");

        std::filesystem::path originalPath(originalFilePathCStr);
        auto parentDir = originalPath.parent_path();
        auto stem = originalPath.stem().string();
        
        std::filesystem::path reacomaFolder = parentDir / "reacoma";
        std::filesystem::create_directory(reacomaFolder);

        std::string takeName = stem + "_" + ss.str() + "_" + suffix;
        std::string newFilename = takeName + ".wav";
        std::filesystem::path outputFilePath = reacomaFolder / newFilename;

        struct WavConfig { char fourcc[4]; int bit_depth; };
        WavConfig config;
        memcpy(config.fourcc, "evaw", 4);
        config.bit_depth = 32;

        PCM_sink* sink = PCM_Sink_CreateEx(nullptr, outputFilePath.string().c_str(), (const char*)&config, sizeof(config), numChans, sampleRate, true);
        if (sink) {
            sink->WriteDoubles(pointerArray.data(), numFrames, numChans, 0, 1);
            delete sink;
        }

        PCM_source* newSource = PCM_Source_CreateFromFile(outputFilePath.c_str());
        if (newSource) {
            MediaItem_Take* newTake = AddTakeToMediaItem(item);
            if (newTake) {
                GetSetMediaItemTakeInfo(newTake, "P_SOURCE", newSource);
                GetSetMediaItemTakeInfo(newTake, "P_NAME", (char*)takeName.c_str());
            }
        }
    }
};
