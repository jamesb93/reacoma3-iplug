// NoveltySliceAlgorithm.cpp
#include "NoveltySliceAlgorithm.h" // Includes IAlgorithm.h implicitly
#include "ReacomaExtension.h"      // Include the full definition of ReacomaExtension
#include "reaper_plugin_functions.h"
#include "IPlugParameter.h" // Required for IParam

#include "../../dependencies/flucoma-core/include/flucoma/clients/rt/NoveltySliceClient.hpp"
#include "../../dependencies/flucoma-core/include/flucoma/clients/common/FluidContext.hpp"
#include "../VectorBufferAdaptor.h" // Assuming this is still needed for InputBufferT
// Make sure MemoryBufferAdaptor is included if it's used directly and not through another header
// Define the namespace aliases here, as they are specific to this implementation
using namespace fluid;
using namespace client;

// Define the Impl struct for this concrete class
struct NoveltySliceAlgorithm::Impl {
    FluidContext mContext;
    NRTThreadingNoveltySliceClient::ParamSetType mParams;
    NRTThreadingNoveltySliceClient mClient;

    Impl() :
        mContext{},
        mParams{NRTThreadingNoveltySliceClient::getParameterDescriptors(), FluidDefaultAllocator()},
        mClient{mParams, mContext}
    {
        // Any further initialization specific to Flucoma client
    }

    void registerParametersImpl(ReacomaExtension* apiProvider, int baseRegisteredParamIdx) {
        for (int i = 0; i < NoveltySliceAlgorithm::kNumParams; ++i) {
            apiProvider->AddParam();
        }

        // Initialize the parameters
        apiProvider->GetParam(baseRegisteredParamIdx + NoveltySliceAlgorithm::kThreshold)
            ->InitDouble("Threshold", 0.5, 0.0, 1.0, 0.01);
        apiProvider->GetParam(baseRegisteredParamIdx + NoveltySliceAlgorithm::kFilterSize)
            ->InitInt("Filter Size", 3, 3, 100);
        
        IParam* algoParam = apiProvider->GetParam(baseRegisteredParamIdx + NoveltySliceAlgorithm::kAlgorithm);
        algoParam->InitInt("Algorithm", 0, 0, NoveltySliceAlgorithm::kNumAlgorithmOptions - 1);
        algoParam->SetDisplayText(NoveltySliceAlgorithm::kSpectrum, "Spectrum");
        algoParam->SetDisplayText(NoveltySliceAlgorithm::kMFCC, "MFCC");
        algoParam->SetDisplayText(NoveltySliceAlgorithm::kChroma, "Chroma");
        algoParam->SetDisplayText(NoveltySliceAlgorithm::kPitch, "Pitch");
        algoParam->SetDisplayText(NoveltySliceAlgorithm::kLoudness, "Loudness");
    }

    // Modified to take baseRegisteredParamIdx
    bool processItemImpl(MediaItem* item, ReacomaExtension* apiProvider, int baseRegisteredParamIdx) {
        if (!item || !apiProvider) return false;

        MediaItem_Take* take = GetActiveTake(item);
        if (!take) return false;

        PCM_source* source = GetMediaItemTake_Source(take);
        if (!source) return false;

        int sampleRate = GetMediaSourceSampleRate(source);
        int numChannels = GetMediaSourceNumChannels(source);
        double itemLength = GetMediaItemInfo_Value(item, "D_LENGTH");
        double playrate = GetMediaItemTakeInfo_Value(take, "D_PLAYRATE");
        double takeOffset = GetMediaItemTakeInfo_Value(take, "D_STARTOFFS");

        int outSize = static_cast<int>(sampleRate * std::min(itemLength * playrate, source->GetLength() - takeOffset));
        if (outSize <= 0) return false;


        std::vector<double> output;
        output.resize(outSize);

        PCM_source_transfer_t transfer;
        transfer.time_s = takeOffset;
        transfer.samplerate = sampleRate;
        transfer.nch = 1; // Process mono for simplicity here, FluCoMa client handles channel selection
        transfer.length = outSize;
        transfer.samples = output.data();
        transfer.samples_out = 0;
        transfer.midi_events = nullptr;
        transfer.approximate_playback_latency = 0.0;
        transfer.absolute_time_s = 0.0;
        transfer.force_bpm = 0.0;

        if (numChannels > 0) { // Ensure there's at least one channel
             source->GetSamples(&transfer);
        } else {
            return false; // No channels to process
        }


        std::vector<float> output_float(output.size());
        std::transform(output.begin(), output.end(), output_float.begin(),
                       [](double d) { return static_cast<float>(d); });
        
        // Ensure numChannels for VectorBufferAdaptor is at least 1, matching the GetSamples call.
        auto inputBuffer = InputBufferT::type(new fluid::VectorBufferAdaptor(output_float, 1, outSize, sampleRate));


        int estimatedSlices = static_cast<int>(output.size() / 1024);
        if (estimatedSlices == 0) estimatedSlices = 1; // Ensure buffer is not zero-sized
        auto outBuffer = std::make_shared<MemoryBufferAdaptor>(1, estimatedSlices, sampleRate);
        auto outputBuffer = BufferT::type(outBuffer);

        // Get parameters from the API provider using baseRegisteredParamIdx
        auto threshold = apiProvider->GetParam(baseRegisteredParamIdx + NoveltySliceAlgorithm::kThreshold)->Value();
        auto filtersize = apiProvider->GetParam(baseRegisteredParamIdx + NoveltySliceAlgorithm::kFilterSize)->Value(); // This is an int
        auto algorithm = apiProvider->GetParam(baseRegisteredParamIdx + NoveltySliceAlgorithm::kAlgorithm)->Value(); // This is an int (enum value)

        mParams.template set<0>(std::move(inputBuffer), nullptr);   // source buffer
        mParams.template set<1>(LongT::type(0), nullptr);           // startFrame
        mParams.template set<2>(LongT::type(-1), nullptr);          // numFrames (-1 = all)
        mParams.template set<3>(LongT::type(0), nullptr);           // startChan
        mParams.template set<4>(LongT::type(-1), nullptr);          // numChans (-1 = all)
        mParams.template set<5>(std::move(outputBuffer), nullptr);  // indices buffer
        mParams.template set<6>(LongT::type(static_cast<long>(algorithm)), nullptr); // algorithm
        mParams.template set<7>(LongRuntimeMaxParam(3, 3), nullptr); // kernelSize (fixed for now or make it a param)
        mParams.template set<8>(FloatT::type(static_cast<float>(threshold)), nullptr); // threshold
        mParams.template set<9>(LongRuntimeMaxParam(static_cast<long>(filtersize), static_cast<long>(filtersize)), nullptr); // filterSize
        mParams.template set<10>(LongT::type(2), nullptr);          // minSliceLength
        mParams.template set<11>(fluid::client::FFTParams(1024, -1, -1), nullptr); // fftSettings (using default hop)

        // Re-create or update client if params might change in a way that requires re-creation
        // For NRTThreadingNoveltySliceClient, it might be fine to reuse if only values change,
        // but if buffer references or fundamental FFT settings change, re-creation might be safer.
        // mClient = NRTThreadingNoveltySliceClient(mParams, mContext); // Potentially re-create
        
        mClient.setSynchronous(true);
        mClient.enqueue(mParams); // Enqueue with current parameters
        Result result = mClient.process();

        if (!result.ok()) {
            // Handle error, e.g., log result.message()
            return false;
        }

        BufferAdaptor::ReadAccess reader(outputBuffer.get());
        if(!reader.exists() || !reader.valid()) return false;
        auto view = reader.samps(0);

        Undo_BeginBlock2(0);

        int markerCount = GetNumTakeMarkers(take);
        for (int i = markerCount - 1; i >= 0; i--) {
            DeleteTakeMarker(take, i);
        }

        const char* markerLabel = ""; // Keep empty or allow customization

        for (fluid::index i = 0; i < view.size(); i++) {
            if (view(i) > 0) { // FluCoMa indices are sample indices
                double markerTimeInSeconds = static_cast<double>(view(i)) / sampleRate;
                // Adjust for take playrate if markers are relative to item timeline
                double markerPositionInTake = markerTimeInSeconds / playrate;
                SetTakeMarker(take, -1, markerLabel, &markerPositionInTake, nullptr);
            }
        }

        UpdateTimeline();
        Undo_EndBlock2(0, "Reacoma: Novelty Slice", -1); // More descriptive undo message

        return true;
    }
};

// Implement constructor for NoveltySliceAlgorithm
NoveltySliceAlgorithm::NoveltySliceAlgorithm(ReacomaExtension* apiProvider)
    : IAlgorithm(apiProvider), // Call base class constructor
      mImpl(std::make_unique<Impl>()) {} // Initialize Impl specific to this class

NoveltySliceAlgorithm::~NoveltySliceAlgorithm() = default;

// Implement pure virtual functions from IAlgorithm
bool NoveltySliceAlgorithm::ProcessItem(MediaItem* item) {
    // Pass mApiProvider and the stored mBaseParamIdx from the base class to the Impl's method
    return mImpl->processItemImpl(item, mApiProvider, mBaseParamIdx);
}

const char* NoveltySliceAlgorithm::GetName() const {
    return "Novelty Slice Algorithm"; // Specific name for this implementation
}

void NoveltySliceAlgorithm::RegisterParameters() {
    // Set the base index for this algorithm's parameters
    mBaseParamIdx = mApiProvider->NParams();
    // Pass mApiProvider and the new mBaseParamIdx to the Impl's method
    mImpl->registerParametersImpl(mApiProvider, mBaseParamIdx);
}
