#include "HPSSAlgorithm.h" // Includes IAlgorithm.h implicitly
#include "ReacomaExtension.h"       // Include the full definition of ReacomaExtension
#include "reaper_plugin_functions.h"
#include "IPlugParameter.h" // Required for IParam
#include "../VectorBufferAdaptor.h" // Ensure this path is correct

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

    // Content from former registerParametersImpl
    for (int i = 0; i < HPSSAlgorithm::kNumParams; ++i) {
        mApiProvider->AddParam();
    }

    mApiProvider->GetParam(mBaseParamIdx + HPSSAlgorithm::kHarmFilterSize)
        ->InitInt("Harmonic Filter Size", 17, 3, 51); // Must be odd

    mApiProvider->GetParam(mBaseParamIdx + HPSSAlgorithm::kPercFilterSize)
        ->InitInt("Percussive Filter Size", 31, 3, 51); // Must be odd
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
    double effectiveTakeDuration = itemLength * playrate; // This is how long the take *sounds* due to playrate
    double actualDurationToProcess = std::min(effectiveTakeDuration, sourceDuration - takeOffset);


    int frameCount = static_cast<int>(sampleRate * actualDurationToProcess); // Renamed outSize to frameCount
    if (frameCount <= 0) return false;

    std::vector<double> inputAudioSamplesAsDouble;
    inputAudioSamplesAsDouble.resize(frameCount);
    
    PCM_source_transfer_t transfer{};
    transfer.time_s = takeOffset;
    transfer.samplerate = static_cast<double>(sampleRate);
    transfer.nch = 1; // HPSS typically processes mono, or per-channel
    transfer.length = frameCount;
    transfer.samples = inputAudioSamplesAsDouble.data();

    if (numChannels > 0) {
        source->GetSamples(&transfer);
    } else {
        return false;
    }

    std::vector<float> inputAudioSamplesAsFloat(frameCount);
    std::transform(
        inputAudioSamplesAsDouble.begin(),
        inputAudioSamplesAsDouble.end(),
        inputAudioSamplesAsFloat.begin(),
        [](double d) { return static_cast<float>(d); });
    
    // Input buffer for FluCoMa
    auto inputBuffer = InputBufferT::type(new fluid::VectorBufferAdaptor(inputAudioSamplesAsFloat, 1, frameCount, sampleRate));

    // Output buffers for harmonic and percussive components
    auto harmMemoryBuffer = std::make_shared<MemoryBufferAdaptor>(1, frameCount, sampleRate);
    auto percMemoryBuffer = std::make_shared<MemoryBufferAdaptor>(1, frameCount, sampleRate);
    auto harmOutputBuffer = fluid::client::BufferT::type(harmMemoryBuffer);
    auto percOutputBuffer = fluid::client::BufferT::type(percMemoryBuffer);
    
    auto harmFilterSizeParam = mApiProvider->GetParam(mBaseParamIdx + HPSSAlgorithm::kHarmFilterSize)->Value();
    auto percFilterSizeParam = mApiProvider->GetParam(mBaseParamIdx + HPSSAlgorithm::kPercFilterSize)->Value();

    // Ensure filter sizes are odd
    if (static_cast<int>(harmFilterSizeParam) % 2 == 0) harmFilterSizeParam +=1;
    if (static_cast<int>(percFilterSizeParam) % 2 == 0) percFilterSizeParam +=1;

    mParams.template set<0>(std::move(inputBuffer), nullptr);    // source
    mParams.template set<1>(LongT::type(0), nullptr);            // startChan
    mParams.template set<2>(LongT::type(-1), nullptr);           // numChans
    mParams.template set<3>(LongT::type(0), nullptr);            // startFrame
    mParams.template set<4>(LongT::type(-1), nullptr);           // numFrames
    mParams.template set<5>(std::move(harmOutputBuffer), nullptr); // harmonic
    mParams.template set<6>(std::move(percOutputBuffer), nullptr); // percussive
    mParams.template set<7>(nullptr, nullptr);                   // residual (optional, not used here)
    mParams.template set<8>(LongRuntimeMaxParam(harmFilterSizeParam, harmFilterSizeParam), nullptr);  // harmFilterSize
    mParams.template set<9>(LongRuntimeMaxParam(percFilterSizeParam, percFilterSizeParam), nullptr);  // percFilterSize
    mParams.template set<10>(LongT::type(0), nullptr);           // mask (0 = binary, 1 = soft)
    mParams.template set<11>(FloatPairsArrayT::type(0,1,1,1), nullptr); // harmSlope
    mParams.template set<12>(FloatPairsArrayT::type(1,0,1,1), nullptr); // percSlope
    mParams.template set<13>(fluid::client::FFTParams(1024, -1, -1), nullptr); // fftsettings
    
    mClient = NRTThreadedHPSSClient(mParams, mContext);
    mClient.setSynchronous(true);
    mClient.enqueue(mParams); // Enqueue potentially updated params
    Result result = mClient.process();

    if (!result.ok()) {
        return false;
    }
    return true;
}

const char* HPSSAlgorithm::GetName() const {
    return "Harmonic Percussive Source Separation";
}

int HPSSAlgorithm::GetNumAlgorithmParams() const {
    return kNumParams;
}
