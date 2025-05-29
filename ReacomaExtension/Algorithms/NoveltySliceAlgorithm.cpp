// NoveltySliceAlgorithm.cpp
#include "NoveltySliceAlgorithm.h" // Includes IAlgorithm.h implicitly
#include "ReacomaExtension.h"      // Include the full definition of ReacomaExtension
#include "reaper_plugin_functions.h"
#include "IPlugParameter.h" // Required for IParam

#include "../../dependencies/flucoma-core/include/flucoma/clients/rt/NoveltySliceClient.hpp"
#include "../../dependencies/flucoma-core/include/flucoma/clients/common/FluidContext.hpp"
#include "../VectorBufferAdaptor.h"

using namespace fluid;
using namespace client;

struct NoveltySliceAlgorithm::Impl {
    FluidContext mContext;
    NRTThreadingNoveltySliceClient::ParamSetType mParams;
    NRTThreadingNoveltySliceClient mClient;

    Impl() :
        mContext{},
        mParams{NRTThreadingNoveltySliceClient::getParameterDescriptors(), FluidDefaultAllocator()},
        mClient{mParams, mContext}
    {
    }

    void registerParametersImpl(ReacomaExtension* apiProvider, int baseRegisteredParamIdx) {
        for (int i = 0; i < NoveltySliceAlgorithm::kNumParams; ++i) {
            apiProvider->AddParam();
        }

        apiProvider->GetParam(baseRegisteredParamIdx + NoveltySliceAlgorithm::kThreshold)
        ->InitDouble("Threshold", 0.5, 0.0, 1.0, 0.01);
        apiProvider->GetParam(baseRegisteredParamIdx + NoveltySliceAlgorithm::kFilterSize)
        ->InitInt("Filter Size", 1, 1, 31);
        apiProvider->GetParam(baseRegisteredParamIdx + NoveltySliceAlgorithm::kKernelSize)
        ->InitInt("Kernel Size", 3, 3, 101);
        apiProvider->GetParam(baseRegisteredParamIdx + NoveltySliceAlgorithm::kMinSliceLength)
        ->InitInt("Minimum Slice Length", 2, 2, 100);
        
        IParam* algoParam = apiProvider->GetParam(baseRegisteredParamIdx + NoveltySliceAlgorithm::kAlgorithm);
        algoParam->InitEnum("Algorithm", NoveltySliceAlgorithm::kSpectrum, NoveltySliceAlgorithm::kNumAlgorithmOptions);
        algoParam->SetDisplayText(NoveltySliceAlgorithm::kSpectrum, "Spectrum");
        algoParam->SetDisplayText(NoveltySliceAlgorithm::kMFCC, "MFCC");
        algoParam->SetDisplayText(NoveltySliceAlgorithm::kChroma, "Chroma");
        algoParam->SetDisplayText(NoveltySliceAlgorithm::kPitch, "Pitch");
        algoParam->SetDisplayText(NoveltySliceAlgorithm::kLoudness, "Loudness");
    }

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
        transfer.nch = 1;
        transfer.length = outSize;
        transfer.samples = output.data();
        transfer.samples_out = 0;
        transfer.midi_events = nullptr;
        transfer.approximate_playback_latency = 0.0;
        transfer.absolute_time_s = 0.0;
        transfer.force_bpm = 0.0;

        if (numChannels > 0) {
             source->GetSamples(&transfer);
        } else {
            return false;
        }

        std::vector<float> output_float(output.size());
        std::transform(output.begin(), output.end(), output_float.begin(),
                       [](double d) { return static_cast<float>(d); });
        
        auto inputBuffer = InputBufferT::type(new fluid::VectorBufferAdaptor(output_float, 1, outSize, sampleRate));

        int estimatedSlices = static_cast<int>(output.size() / 1024);
        if (estimatedSlices == 0) estimatedSlices = 1;
        auto outBuffer = std::make_shared<MemoryBufferAdaptor>(1, estimatedSlices, sampleRate);
        auto outputBuffer = fluid::client::BufferT::type(outBuffer);

        auto threshold = apiProvider->GetParam(baseRegisteredParamIdx + NoveltySliceAlgorithm::kThreshold)->Value();
        auto filtersize = apiProvider->GetParam(baseRegisteredParamIdx + NoveltySliceAlgorithm::kFilterSize)->Value();
        auto algorithm = apiProvider->GetParam(baseRegisteredParamIdx + NoveltySliceAlgorithm::kAlgorithm)->Value();

        mParams.template set<0>(std::move(inputBuffer), nullptr);
        mParams.template set<1>(LongT::type(0), nullptr);
        mParams.template set<2>(LongT::type(-1), nullptr);
        mParams.template set<3>(LongT::type(0), nullptr);
        mParams.template set<4>(LongT::type(-1), nullptr);
        mParams.template set<5>(std::move(outputBuffer), nullptr);
        mParams.template set<6>(LongT::type(static_cast<long>(algorithm)), nullptr);
        mParams.template set<7>(LongRuntimeMaxParam(3, 3), nullptr);
        mParams.template set<8>(FloatT::type(static_cast<float>(threshold)), nullptr);
        mParams.template set<9>(LongRuntimeMaxParam(static_cast<long>(filtersize), static_cast<long>(filtersize)), nullptr);
        mParams.template set<10>(LongT::type(2), nullptr);
        mParams.template set<11>(fluid::client::FFTParams(1024, -1, -1), nullptr);
        
        mClient = NRTThreadingNoveltySliceClient(mParams, mContext);
        mClient.setSynchronous(true);
        mClient.enqueue(mParams);
        Result result = mClient.process();

        if (!result.ok()) {
            return false;
        }

        BufferAdaptor::ReadAccess reader(outputBuffer.get());
        if(!reader.exists() || !reader.valid()) return false;
        auto view = reader.samps(0);

        int markerCount = GetNumTakeMarkers(take);
        for (int i = markerCount - 1; i >= 0; i--) {
            DeleteTakeMarker(take, i);
        }

        const char* markerLabel = "";

        for (fluid::index i = 0; i < view.size(); i++) {
            if (view(i) > 0) {
                double markerTimeInSeconds = static_cast<double>(view(i)) / sampleRate;
                double markerPositionInTake = markerTimeInSeconds / playrate;
                SetTakeMarker(take, -1, markerLabel, &markerPositionInTake, nullptr);
            }
        }
        return true;
    }
};

NoveltySliceAlgorithm::NoveltySliceAlgorithm(ReacomaExtension* apiProvider)
    : IAlgorithm(apiProvider),
      mImpl(std::make_unique<Impl>()) {}

NoveltySliceAlgorithm::~NoveltySliceAlgorithm() = default;

bool NoveltySliceAlgorithm::ProcessItem(MediaItem* item) {
    return mImpl->processItemImpl(item, mApiProvider, mBaseParamIdx);
}

const char* NoveltySliceAlgorithm::GetName() const {
    return "Novelty Slice";
}

void NoveltySliceAlgorithm::RegisterParameters() {
    mBaseParamIdx = mApiProvider->NParams();
    mImpl->registerParametersImpl(mApiProvider, mBaseParamIdx);
}

int NoveltySliceAlgorithm::GetNumAlgorithmParams() const {
    return kNumParams;
}
