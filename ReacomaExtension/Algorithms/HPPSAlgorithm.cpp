#include "HPSSAlgorithm.h"
#include "ReacomaExtension.h"
#include "reaper_plugin_functions.h"
#include "IPlugParameter.h"
#include "../VectorBufferAdaptor.h"
#include "InMemoryDecoder.h"
#include <filesystem>

using namespace fluid;
using namespace client;

HPSSAlgorithm::HPSSAlgorithm(ReacomaExtension* apiProvider)
    : IAlgorithm(apiProvider),
      mContext{},
      mParams{NRTThreadedHPSSClient::getParameterDescriptors(), FluidDefaultAllocator()},
      mClient{mParams, mContext}
{
}

HPSSAlgorithm::~HPSSAlgorithm() = default;

void HPSSAlgorithm::RegisterParameters() {
    mBaseParamIdx = mApiProvider->NParams();

    for (int i = 0; i < HPSSAlgorithm::kNumParams; ++i) {
        mApiProvider->AddParam();
    }

    mApiProvider->GetParam(mBaseParamIdx + HPSSAlgorithm::kHarmFilterSize)
        ->InitInt("Harmonic Filter Size", 17, 3, 51);

    mApiProvider->GetParam(mBaseParamIdx + HPSSAlgorithm::kPercFilterSize)
        ->InitInt("Percussive Filter Size", 31, 3, 51);
}

bool HPSSAlgorithm::ProcessItem(MediaItem* item) {
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
    
    auto harmFilterSizeParam = mApiProvider->GetParam(mBaseParamIdx + HPSSAlgorithm::kHarmFilterSize)->Value();
    auto percFilterSizeParam = mApiProvider->GetParam(mBaseParamIdx + HPSSAlgorithm::kPercFilterSize)->Value();
    
    auto inputBuffer = InputBufferT::type(new fluid::VectorBufferAdaptor(allChannelsAsFloat, numChannels, frameCount, sampleRate));

    auto harmMemoryBuffer = std::make_shared<MemoryBufferAdaptor>(1, frameCount, sampleRate);
    auto percMemoryBuffer = std::make_shared<MemoryBufferAdaptor>(1, frameCount, sampleRate);
    auto harmOutputBuffer = fluid::client::BufferT::type(harmMemoryBuffer);
    auto percOutputBuffer = fluid::client::BufferT::type(percMemoryBuffer);
    

    if (static_cast<int>(harmFilterSizeParam) % 2 == 0) harmFilterSizeParam +=1;
    if (static_cast<int>(percFilterSizeParam) % 2 == 0) percFilterSizeParam +=1;

    mParams.template set<0>(std::move(inputBuffer), nullptr);    // source
    mParams.template set<1>(LongT::type(0), nullptr);            // startChan
    mParams.template set<2>(LongT::type(-1), nullptr);           // numChans
    mParams.template set<3>(LongT::type(0), nullptr);            // startFrame
    mParams.template set<4>(LongT::type(-1), nullptr);           // numFrames
    mParams.template set<5>(std::move(harmOutputBuffer), nullptr); // harmonic
    mParams.template set<6>(std::move(percOutputBuffer), nullptr); // percussive
    mParams.template set<7>(nullptr, nullptr);                   
    mParams.template set<8>(LongRuntimeMaxParam(harmFilterSizeParam, harmFilterSizeParam), nullptr);
    mParams.template set<9>(LongRuntimeMaxParam(percFilterSizeParam, percFilterSizeParam), nullptr); 
    mParams.template set<10>(LongT::type(0), nullptr);           
    mParams.template set<11>(FloatPairsArrayT::type(0,1,1,1), nullptr);
    mParams.template set<12>(FloatPairsArrayT::type(1,0,1,1), nullptr);
    mParams.template set<13>(fluid::client::FFTParams(1024, -1, -1), nullptr);
    
    mClient = NRTThreadedHPSSClient(mParams, mContext);
    mClient.setSynchronous(true);
    mClient.enqueue(mParams);
    Result result = mClient.process();

    if (!result.ok()) {
        return false;
    }
    
    AddOutputToTake(item, harmOutputBuffer, numChannels, sampleRate, "harmonic");
    AddOutputToTake(item, percOutputBuffer, numChannels, sampleRate, "percussive");
    
    UpdateTimeline();
    
    return true;
}

void HPSSAlgorithm::AddOutputToTake(MediaItem* item, BufferT::type output, int numChannels, int sampleRate, const std::string& suffix) {
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

const char* HPSSAlgorithm::GetName() const {
    return "Harmonic Percussive Source Separation";
}

int HPSSAlgorithm::GetNumAlgorithmParams() const {
    return kNumParams;
}
