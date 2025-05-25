// NoveltySliceAlgorithm.cpp
#include "NoveltySliceAlgorithm.h" // Includes IAlgorithm.h implicitly
#include "ReacomaExtension.h"      // Include the full definition of ReacomaExtension
#include "reaper_plugin_functions.h"


#include "../../dependencies/flucoma-core/include/flucoma/clients/rt/NoveltySliceClient.hpp"
#include "../../dependencies/flucoma-core/include/flucoma/clients/common/FluidContext.hpp"
#include "../VectorBufferAdaptor.h" // Assuming this is still needed

// Define the namespace aliases here, as they are specific to this implementation
using namespace fluid;
using namespace client;

// Define the Impl struct for this concrete class
struct NoveltySliceAlgorithm::Impl {
    FluidContext mContext;
    NRTThreadingNoveltySliceClient::ParamSetType mParams;
    NRTThreadingNoveltySliceClient mClient;

    Impl() : // No longer needs apiProvider in its constructor
        mContext{},
        mParams{NRTThreadingNoveltySliceClient::getParameterDescriptors(), FluidDefaultAllocator()},
        mClient{mParams, mContext}
    {
        // Any further initialization specific to Flucoma client
    }

    void registerParametersImpl(ReacomaExtension* apiProvider) {
    }

    bool processItemImpl(MediaItem* item, ReacomaExtension* apiProvider) {
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

        int outSize = sampleRate * std::min(itemLength * playrate, source->GetLength() - takeOffset);

        std::vector<double> output;
        output.resize(outSize);

        PCM_source_transfer_t transfer;
        transfer.time_s = takeOffset;
        transfer.samplerate = sampleRate;
        transfer.nch = 1;
        transfer.length = outSize;
        transfer.samples = output.data();
        transfer.samples_out = 0;
        transfer.midi_events = nullptr;
        transfer.approximate_playback_latency = 0.0;
        transfer.absolute_time_s = 0.0;
        transfer.force_bpm = 0.0;

        if (numChannels == 1) {
            source->GetSamples(&transfer);
        }

        std::vector<float> output_float(output.size());
        std::transform(output.begin(), output.end(), output_float.begin(),
        [](double d) { return static_cast<float>(d); });
        auto inputBuffer = InputBufferT::type(new fluid::VectorBufferAdaptor(output_float, numChannels, outSize, sampleRate));

        int estimatedSlices = static_cast<int>(output.size() / 1024);
        auto outBuffer = std::make_shared<MemoryBufferAdaptor>(1, estimatedSlices, sampleRate);
        auto outputBuffer = BufferT::type(outBuffer);

        // Get parameters from the API provider via the base class pointer
        auto threshold = apiProvider->GetParam(0)->Value();
        auto filtersize = apiProvider->GetParam(1)->Value();
        auto algorithm = apiProvider->GetParam(2)->Value();

        mParams.template set<0>(std::move(inputBuffer), nullptr);  // source buffer
        mParams.template set<1>(LongT::type(0), nullptr);           // startFrame
        mParams.template set<2>(LongT::type(-1), nullptr);          // numFrames (-1 = all)
        mParams.template set<3>(LongT::type(0), nullptr);           // startChan
        mParams.template set<4>(LongT::type(-1), nullptr);          // numChans (-1 = all)
        mParams.template set<5>(std::move(outputBuffer), nullptr); // indices buffer
        mParams.template set<6>(LongT::type(algorithm), nullptr);   // algorithm
        mParams.template set<7>(LongRuntimeMaxParam(3, 3), nullptr); // kernelSize
        mParams.template set<8>(FloatT::type(threshold), nullptr); // threshold
        mParams.template set<9>(LongRuntimeMaxParam(filtersize, filtersize), nullptr); // filterSize
        mParams.template set<10>(LongT::type(2), nullptr);          // minSliceLength
        mParams.template set<11>(fluid::client::FFTParams(1024, -1, -1), nullptr); // hopSize

        mClient = NRTThreadingNoveltySliceClient(mParams, mContext);
        mClient.setSynchronous(true);
        mClient.enqueue(mParams);
        Result result = mClient.process();

        BufferAdaptor::ReadAccess reader(outputBuffer.get());
        auto view = reader.samps(0);

        Undo_BeginBlock2(0);

        int markerCount = GetNumTakeMarkers(take);
        for (int i = markerCount - 1; i >= 0; i--) {
            DeleteTakeMarker(take, i);
        }

        int numMarkers = 0;
        const char* markerLabel = "";

        for (fluid::index i = 0; i < view.size(); i++) {
            if (view(i) > 0) {
                double markerTime = view(i) / sampleRate / playrate;
                SetTakeMarker(take, -1, markerLabel, &markerTime, nullptr);
                numMarkers++;
            }
        }

        UpdateTimeline();
        Undo_EndBlock2(0, "reacoma", -1);

        return true;
    }
};

// Implement constructor for NoveltySliceAlgorithm
NoveltySliceAlgorithm::NoveltySliceAlgorithm(ReacomaExtension* apiProvider)
    : IAlgorithm(apiProvider), // Call base class constructor
      mImpl(std::make_unique<Impl>()) {} // Initialize Impl specific to this class

NoveltySliceAlgorithm::~NoveltySliceAlgorithm() = default;

// Implement the pure virtual functions from IAlgorithm
bool NoveltySliceAlgorithm::ProcessItem(MediaItem* item) {
    // Pass mApiProvider from the base class to the Impl's method
    return mImpl->processItemImpl(item, mApiProvider);
}

const char* NoveltySliceAlgorithm::GetName() const {
    return "Novelty Slice Algorithm"; // Specific name for this implementation
}

void NoveltySliceAlgorithm::RegisterParameters() {
    // Pass mApiProvider from the base class to the Impl's method
    return mImpl->registerParametersImpl(mApiProvider);
}
