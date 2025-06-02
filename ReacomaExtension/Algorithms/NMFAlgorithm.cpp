#include "NMFAlgorithm.h"
#include "ReacomaExtension.h"
#include "reaper_plugin_functions.h"
#include "IPlugParameter.h"
#include "../VectorBufferAdaptor.h"
#include "InMemoryDecoder.h"
#include <filesystem>

using namespace fluid;
using namespace client;

NMFAlgorithm::NMFAlgorithm(ReacomaExtension* apiProvider)
    : IAlgorithm(apiProvider),
      mContext{},
      mParams{NRTThreadedNMFClient::getParameterDescriptors(), FluidDefaultAllocator()},
      mClient{mParams, mContext}
{
}

NMFAlgorithm::~NMFAlgorithm() = default;

void NMFAlgorithm::RegisterParameters() {
    mBaseParamIdx = mApiProvider->NParams();

    for (int i = 0; i < NMFAlgorithm::kNumParams; ++i) {
        mApiProvider->AddParam();
    }
    
    mApiProvider->GetParam(mBaseParamIdx + NMFAlgorithm::kComponents)
    ->InitInt("Number of Components", 2, 2, 10);

    mApiProvider->GetParam(mBaseParamIdx + NMFAlgorithm::kIterations)
    ->InitInt("Number of Iterations", 100, 1, 1000);
}

bool NMFAlgorithm::ProcessItem(MediaItem* item) {
    if (!item || !mApiProvider) return false;

    MediaItem_Take* take = GetActiveTake(item);
    if (!take) return false;

    PCM_source* source = GetMediaItemTake_Source(take);
    if (!source) return false;

    int sampleRate = GetMediaSourceSampleRate(source);
    int numChannels = GetMediaSourceNumChannels(source);
    double itemLength = GetMediaItemInfo_Value(item, "D_LENGTH");
    double playrate = GetMediaItemTakeInfo_Value(take, "D_PLAYRATE");
    double takeOffset = GetMediaItemTakeInfo_Value(take, "D_STARTOFFS");
    
    double sourceDuration = source->GetLength();
    double effectiveTakeDuration = itemLength * playrate;
    double actualDurationToProcess = std::min(effectiveTakeDuration, sourceDuration - takeOffset);

    int frameCount = static_cast<int>(sampleRate * actualDurationToProcess);
    if (frameCount <= 0 || numChannels <= 0) return false;

    std::vector<double> allChannelsAsDouble(frameCount * numChannels);
    
    PCM_source_transfer_t transfer{};
    transfer.time_s = takeOffset;
    transfer.samplerate = static_cast<double>(sampleRate);
    transfer.nch = numChannels;
    transfer.length = frameCount;
    transfer.samples = allChannelsAsDouble.data();
    source->GetSamples(&transfer);
    
    std::vector<float> allChannelsAsFloat(allChannelsAsDouble.begin(), allChannelsAsDouble.end());
    
    auto componentsParam = mApiProvider->GetParam(mBaseParamIdx + NMFAlgorithm::kComponents)->Value();
    auto iterationsParam = mApiProvider->GetParam(mBaseParamIdx + NMFAlgorithm::kIterations)->Value();

    auto inputBuffer = InputBufferT::type(new fluid::VectorBufferAdaptor(allChannelsAsFloat, numChannels, frameCount, sampleRate));
    
    auto resynthMemoryBuffer = std::make_shared<MemoryBufferAdaptor>(numChannels * componentsParam, frameCount, sampleRate);
    auto resynthOutputBuffer = fluid::client::BufferT::type(resynthMemoryBuffer);

    mParams.template set<0>(std::move(inputBuffer), nullptr);
    mParams.template set<1>(LongT::type(0), nullptr);
    mParams.template set<2>(LongT::type(-1), nullptr);
    mParams.template set<3>(LongT::type(0), nullptr);
    mParams.template set<4>(LongT::type(-1), nullptr);
    mParams.template set<5>(std::move(resynthOutputBuffer), nullptr);
    mParams.template set<6>(LongT::type(1), nullptr);
    mParams.template set<11>(componentsParam, nullptr);
    mParams.template set<12>(iterationsParam, nullptr);
    mParams.template set<13>(fluid::client::FFTParams(1024, -1, -1), nullptr);

    mClient = NRTThreadedNMFClient(mParams, mContext);
    mClient.setSynchronous(true);
    mClient.enqueue(mParams); 
    Result result = mClient.process();

    if (!result.ok()) {
        ShowConsoleMsg("NMF processing failed.\n");
        return false;
    }
    
    AddOutputToTake(item, resynthOutputBuffer, numChannels * componentsParam, sampleRate, "nmf_components");
    
    UpdateTimeline();
    
    return true;
}

void NMFAlgorithm::AddOutputToTake(MediaItem* item, BufferT::type output, int numChannels, int sampleRate, const std::string& suffix) {
    fluid::client::BufferAdaptor::ReadAccess bufferReader(output.get());
    auto numFrames = bufferReader.numFrames();
    auto numChans = bufferReader.numChans();
    
    std::vector<ReaSample> audioVector;
    audioVector.resize(bufferReader.numChans() * bufferReader.numFrames());

    std::vector<std::vector<ReaSample>> channelData(numChans, std::vector<ReaSample>(numFrames));
    std::vector<ReaSample*> pointerArray(numChans);

    const float* sourceData = bufferReader.allFrames().data();

    for (int i = 0; i < numChans; ++i) {
        for (int j = 0; j < numFrames; ++j) {
            channelData[i][j] = static_cast<ReaSample>(sourceData[j * numChans + i]);
        }
        pointerArray[i] = channelData[i].data();
    }

    char projectPath[4096];
    GetProjectPath(projectPath, sizeof(projectPath));
    
    char originalFilePathCStr[4096] = "";
    auto activeTake = GetActiveTake(item);
    if (activeTake) {
        auto takeSource = GetMediaItemTake_Source(activeTake);
        if (takeSource) {
            auto srcParent = GetMediaSourceParent(takeSource);
            GetMediaSourceFileName(srcParent ? srcParent : takeSource, originalFilePathCStr, sizeof(originalFilePathCStr));
        }
    }
    
    std::filesystem::path outputFilePath;
    std::string takeName;
    
    std::filesystem::path originalPath(originalFilePathCStr);

    auto parentDir = originalPath.parent_path();
    auto stem = originalPath.stem().string();
    auto extension = originalPath.extension().string();

    takeName = stem + "_" + suffix;
    std::string newFilename = takeName + ".wav";
    outputFilePath = parentDir / newFilename;

    struct WavConfig {
        char fourcc[4];
        int bit_depth;
    };
    
    WavConfig config;
    memcpy(config.fourcc, "evaw", 4);
    config.bit_depth = 32;

    PCM_sink* sink = PCM_Sink_CreateEx(
        nullptr,
        outputFilePath.string().c_str(),
        (const char*)&config,
        sizeof(config),
        numChans,
        sampleRate,
        true
    );
    sink->WriteDoubles(pointerArray.data(), numFrames, numChans, 0, 1);
    delete sink;
    
    PCM_source* newSource = PCM_Source_CreateFromFile(outputFilePath.c_str());
    if (newSource) {
        MediaItem_Take* take = AddTakeToMediaItem(item);
        if (take) {
            GetSetMediaItemTakeInfo(take, "P_SOURCE", newSource);
            GetSetMediaItemTakeInfo(take, "P_NAME", (char*)takeName.c_str());
        }
    }
}

const char* NMFAlgorithm::GetName() const {
    return "Non-negative Matrix Factorisation";
}

int NMFAlgorithm::GetNumAlgorithmParams() const {
    return kNumParams;
}
