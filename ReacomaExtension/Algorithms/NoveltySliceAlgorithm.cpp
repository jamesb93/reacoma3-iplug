// NoveltySliceAlgorithm.cpp
#include "NoveltySliceAlgorithm.h"
#include "ReacomaExtension.h" // Include the full definition of ReacomaExtension
#include "reaper_plugin_functions.h"

#include "../../dependencies/flucoma-core/include/flucoma/clients/rt/NoveltySliceClient.hpp"
#include "../../dependencies/flucoma-core/include/flucoma/clients/common/FluidContext.hpp"
#include "../VectorBufferAdaptor.h" // Make sure this path is correct

using namespace fluid;
using namespace client;

struct NoveltySliceAlgorithm::Impl {
    FluidContext mContext;
    NRTThreadingNoveltySliceClient::ParamSetType mParams;
    NRTThreadingNoveltySliceClient mClient;
    ReacomaExtension* mApiProvider; // Changed to ReacomaExtension*

    Impl(ReacomaExtension* apiProvider) : // Changed
        mContext{},
        mParams{NRTThreadingNoveltySliceClient::getParameterDescriptors(), FluidDefaultAllocator()},
        mClient{mParams, mContext},
        mApiProvider(apiProvider) // Changed
    {
        // Any further initialization
    }

    bool processItemImpl(MediaItem* item) {
        if (!item || !mApiProvider) return false; // Check if pointer is valid

        // Use the ReacomaExtension pointer to call API functions
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
        // Setup parameters
        std::vector<float> output_float(output.size());
        std::transform(output.begin(), output.end(), output_float.begin(),
        [](double d) { return static_cast<float>(d); });
        auto inputBuffer = InputBufferT::type(new fluid::VectorBufferAdaptor(output_float, numChannels, outSize, sampleRate));
    
        int estimatedSlices = static_cast<int>(output.size() / 1024);
        auto outBuffer = std::make_shared<MemoryBufferAdaptor>(1, estimatedSlices, sampleRate);
        auto outputBuffer = BufferT::type(outBuffer);
    
        auto threshold = mApiProvider->GetParam(0)->Value();
        auto filtersize = mApiProvider->GetParam(1)->Value();
        auto algorithm = mApiProvider->GetParam(2)->Value();
        
        
        mParams.template set<0>(std::move(inputBuffer), nullptr);  // source buffer
        mParams.template set<1>(LongT::type(0), nullptr);         // startFrame
        mParams.template set<2>(LongT::type(-1), nullptr);        // numFrames (-1 = all)
        mParams.template set<3>(LongT::type(0), nullptr);         // startChan
        mParams.template set<4>(LongT::type(-1), nullptr);        // numChans (-1 = all)
        mParams.template set<5>(std::move(outputBuffer), nullptr); // indices buffer
        mParams.template set<6>(LongT::type(algorithm), nullptr);       // algorithm
        mParams.template set<7>(LongRuntimeMaxParam(3, 3), nullptr); // kernelSize
        mParams.template set<8>(FloatT::type(threshold), nullptr); // threshold
        mParams.template set<9>(LongRuntimeMaxParam(3, 3), nullptr); // filterSize
        mParams.template set<10>(LongT::type(2), nullptr);   // minSliceLength
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

NoveltySliceAlgorithm::NoveltySliceAlgorithm(ReacomaExtension* apiProvider) // Changed
    : mImpl(std::make_unique<Impl>(apiProvider)) {} // Changed

NoveltySliceAlgorithm::~NoveltySliceAlgorithm() = default;

bool NoveltySliceAlgorithm::ProcessItem(MediaItem* item) {
    return mImpl->processItemImpl(item);
}

const char* NoveltySliceAlgorithm::GetName() const {
    return "NoveltySlice";
}
