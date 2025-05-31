// NoveltySliceAlgorithm.cpp
#include "NoveltySliceAlgorithm.h" // Includes IAlgorithm.h implicitly
#include "ReacomaExtension.h"      // Include the full definition of ReacomaExtension
#include "reaper_plugin_functions.h"
#include "IPlugParameter.h"
#include "../VectorBufferAdaptor.h"

using namespace fluid;
using namespace client;

NoveltySliceAlgorithm::NoveltySliceAlgorithm(ReacomaExtension* apiProvider)
    : IAlgorithm(apiProvider),
      mContext{},
      mParams{NRTThreadingNoveltySliceClient::getParameterDescriptors(), FluidDefaultAllocator()},
      mClient{mParams, mContext}
{
}

NoveltySliceAlgorithm::~NoveltySliceAlgorithm() = default;

void NoveltySliceAlgorithm::RegisterParameters() {
    mBaseParamIdx = mApiProvider->NParams(); // Set the base parameter index

    // Content from former registerParametersImpl
    for (int i = 0; i < NoveltySliceAlgorithm::kNumParams; ++i) {
        mApiProvider->AddParam();
    }

    mApiProvider->GetParam(mBaseParamIdx + NoveltySliceAlgorithm::kThreshold)
        ->InitDouble("Threshold", 0.5, 0.0, 1.0, 0.01);
    mApiProvider->GetParam(mBaseParamIdx + NoveltySliceAlgorithm::kFilterSize)
        ->InitInt("Filter Size", 1, 1, 31); // Max 31 based on typical median filter sizes
    mApiProvider->GetParam(mBaseParamIdx + NoveltySliceAlgorithm::kKernelSize)
        ->InitInt("Kernel Size", 3, 3, 101); // Must be odd
    mApiProvider->GetParam(mBaseParamIdx + NoveltySliceAlgorithm::kMinSliceLength)
        ->InitInt("Minimum Slice Length", 2, 2, 100); // In frames or ms depending on context
    
    IParam* algoParam = mApiProvider->GetParam(mBaseParamIdx + NoveltySliceAlgorithm::kAlgorithm);
    algoParam->InitEnum("Algorithm", NoveltySliceAlgorithm::kSpectrum, NoveltySliceAlgorithm::kNumAlgorithmOptions -1); // kNumAlgorithmOptions is count, so max index is count-1
    algoParam->SetDisplayText(NoveltySliceAlgorithm::kSpectrum, "Spectrum");
    algoParam->SetDisplayText(NoveltySliceAlgorithm::kMFCC, "MFCC");
    algoParam->SetDisplayText(NoveltySliceAlgorithm::kChroma, "Chroma");
    algoParam->SetDisplayText(NoveltySliceAlgorithm::kPitch, "Pitch");
    algoParam->SetDisplayText(NoveltySliceAlgorithm::kLoudness, "Loudness");
}

bool NoveltySliceAlgorithm::ProcessItem(MediaItem* item) {
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

    // Ensure effective length calculation considers playrate correctly and doesn't exceed source bounds
    double sourceDuration = source->GetLength();
    double effectiveTakeDuration = itemLength * playrate;
    double actualDurationToProcess = std::min(effectiveTakeDuration, sourceDuration - takeOffset);

    int outSize = static_cast<int>(sampleRate * actualDurationToProcess);
    if (outSize <= 0) return false;

    std::vector<double> audioSamplesAsDouble; // Changed name from 'output' to avoid confusion
    audioSamplesAsDouble.resize(outSize);
    
    PCM_source_transfer_t transfer{}; // Use struct initialization
    transfer.time_s = takeOffset;
    transfer.samplerate = static_cast<double>(sampleRate); // PCM_source_transfer_t expects double
    transfer.nch = 1; // Process mono for simplicity with NoveltySlice
    transfer.length = outSize;
    transfer.samples = audioSamplesAsDouble.data();
    // Other members are default (nullptr or 0.0)

    if (numChannels > 0) {
        source->GetSamples(&transfer);
    } else {
        return false;
    }

    std::vector<float> audioSamplesAsFloat(outSize); // Changed name
    std::transform(audioSamplesAsDouble.begin(), audioSamplesAsDouble.end(), audioSamplesAsFloat.begin(),
                   [](double d) { return static_cast<float>(d); });
    
    auto inputBuffer = InputBufferT::type(new fluid::VectorBufferAdaptor(audioSamplesAsFloat, 1, outSize, sampleRate));

    // Estimate slices: this is a rough guess, FluCoMa buffers can resize if needed, but good to have a starting point
    int estimatedSlices = std::max(1, static_cast<int>(outSize / 1024.0)); // Ensure at least 1
    auto outBuffer = std::make_shared<MemoryBufferAdaptor>(1, estimatedSlices, sampleRate); // Slices buffer
    auto slicesOutputBuffer = fluid::client::BufferT::type(outBuffer); // Changed name for clarity

    // Get parameter values using mApiProvider and mBaseParamIdx
    auto threshold = mApiProvider->GetParam(mBaseParamIdx + NoveltySliceAlgorithm::kThreshold)->Value();
    auto filtersize = mApiProvider->GetParam(mBaseParamIdx + NoveltySliceAlgorithm::kFilterSize)->Value();
    auto kernelsize = mApiProvider->GetParam(mBaseParamIdx + NoveltySliceAlgorithm::kKernelSize)->Value();
    auto minslicelength = mApiProvider->GetParam(mBaseParamIdx + NoveltySliceAlgorithm::kMinSliceLength)->Value();
    auto algorithm = mApiProvider->GetParam(mBaseParamIdx + NoveltySliceAlgorithm::kAlgorithm)->Value();

    // Ensure kernel size is odd
    if (static_cast<int>(kernelsize) % 2 == 0) kernelsize +=1;
    // Ensure filter size is odd for median filter
    if (static_cast<int>(filtersize) % 2 == 0) filtersize +=1;


    mParams.template set<0>(std::move(inputBuffer), nullptr); // source
    mParams.template set<1>(LongT::type(0), nullptr); // startChan
    mParams.template set<2>(LongT::type(-1), nullptr); // numChans
    mParams.template set<3>(LongT::type(0), nullptr); // startFrame
    mParams.template set<4>(LongT::type(-1), nullptr); // numFrames
    mParams.template set<5>(std::move(slicesOutputBuffer), nullptr); // indices
    mParams.template set<6>(LongT::type(static_cast<long>(algorithm)), nullptr); // algorithm
    mParams.template set<7>(LongRuntimeMaxParam(kernelsize, kernelsize), nullptr); // kernelsize
    mParams.template set<8>(FloatT::type(static_cast<float>(threshold)), nullptr); // threshold
    mParams.template set<9>(LongRuntimeMaxParam(filtersize, filtersize), nullptr); // filtersize
    mParams.template set<10>(LongT::type(static_cast<long>(minslicelength)), nullptr); // mininteronsetlength
    mParams.template set<11>(fluid::client::FFTParams(1024, -1, -1), nullptr); // fftsettings
    
    mClient = NRTThreadingNoveltySliceClient(mParams, mContext);
    mClient.setSynchronous(true); // Good for REAPER integration
    mClient.enqueue(mParams); // Enqueue the (potentially updated) params
    Result result = mClient.process();

    if (!result.ok()) {
        return false;
    }

    auto processedSlicesBuffer = mParams.template get<5>().get();
    BufferAdaptor::ReadAccess reader(processedSlicesBuffer);

    if(!reader.exists() || !reader.valid()) return false;
    auto view = reader.samps(0); // Assuming slices are in channel 0

    int markerCount = GetNumTakeMarkers(take);
    for (int i = markerCount - 1; i >= 0; i--) {
        DeleteTakeMarker(take, i); // Consider if this should be optional
    }

    const char* markerLabel = ""; // Or a custom label

    for (fluid::index i = 0; i < view.size(); i++) {
        if (view(i) > 0) { // FluCoMa slice indices are sample indices
            double markerTimeInSeconds = static_cast<double>(view(i)) / sampleRate;
            // Marker position in take needs to account for take's playrate if slices are relative to source samples
            double markerPositionInTake = markerTimeInSeconds; // If view(i) is already in take's timeline post-playrate
                                                               // If view(i) is from original source samples, then:
                                                               // markerPositionInTake = markerTimeInSeconds / playrate;
                                                               // Assuming NoveltySlice operates on the stretched/compressed audio as presented by the take
            
            // Validate marker position against take length
            if (markerPositionInTake < itemLength) { // itemLength is original, playrate adjusted by REAPER
                 SetTakeMarker(take, -1, markerLabel, &markerPositionInTake, nullptr);
            }
        }
    }
    return true;
}

const char* NoveltySliceAlgorithm::GetName() const {
    return "Novelty Slice";
}

int NoveltySliceAlgorithm::GetNumAlgorithmParams() const {
    return kNumParams;
}
