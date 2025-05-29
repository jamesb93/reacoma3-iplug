
#include "HPSSAlgorithm.h" // Includes IAlgorithm.h implicitly
#include "ReacomaExtension.h"      // Include the full definition of ReacomaExtension
#include "reaper_plugin_functions.h"
#include "IPlugParameter.h" // Required for IParam

#include "../../dependencies/flucoma-core/include/flucoma/clients/rt/HPSSClient.hpp"
#include "../../dependencies/flucoma-core/include/flucoma/clients/common/FluidContext.hpp"
#include "../VectorBufferAdaptor.h"

using namespace fluid;
using namespace client;

struct HPSSAlgorithm::Impl {
    FluidContext mContext;
    NRTThreadedHPSSClient::ParamSetType mParams;
    NRTThreadedHPSSClient mClient;

    Impl() :
        mContext{},
        mParams{NRTThreadedHPSSClient::getParameterDescriptors(), FluidDefaultAllocator()},
        mClient{mParams, mContext}
    {
    }

    void registerParametersImpl(ReacomaExtension* apiProvider, int baseRegisteredParamIdx) {
        for (int i = 0; i < HPSSAlgorithm::kNumParams; ++i) {
            apiProvider->AddParam();
        }

        apiProvider->GetParam(baseRegisteredParamIdx + HPSSAlgorithm::kHarmFilterSize)
        ->InitInt("Harmonic Filter Size", 17, 3, 51);

        apiProvider->GetParam(baseRegisteredParamIdx + HPSSAlgorithm::kPercFilterSize)
        ->InitInt("Percussive Filter Size", 31, 3, 51);
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

        std::vector<double> inputAudioSamplesAsDouble;
        inputAudioSamplesAsDouble.resize(outSize);
        
        PCM_source_transfer_t transfer;
        transfer.time_s = takeOffset;
        transfer.samplerate = sampleRate;
        transfer.nch = 1;
        transfer.length = outSize;
        transfer.samples = inputAudioSamplesAsDouble.data();
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

        std::vector<float> inputAudioSamplesAsFloat(outSize);
        std::transform(
                       inputAudioSamplesAsDouble.begin(),
                       inputAudioSamplesAsDouble.end(),
                       inputAudioSamplesAsFloat.begin(),
                       [](double d) { return static_cast<float>(d); });
        
        auto inputBuffer = InputBufferT::type(new fluid::VectorBufferAdaptor(inputAudioSamplesAsFloat, 1, outSize, sampleRate));

        auto harmMemoryBuffer = std::make_shared<MemoryBufferAdaptor>(1, outSize, sampleRate);
        auto percMemoryBuffer = std::make_shared<MemoryBufferAdaptor>(1, outSize, sampleRate);
        auto harmOutputBuffer = fluid::client::BufferT::type(harmMemoryBuffer);
        auto percOutputBuffer = fluid::client::BufferT::type(percMemoryBuffer);
        
        auto harmFilterSize = apiProvider->GetParam(baseRegisteredParamIdx + HPSSAlgorithm::kHarmFilterSize)->Value();
        auto percFilterSize = apiProvider->GetParam(baseRegisteredParamIdx + HPSSAlgorithm::kPercFilterSize)->Value();

        mParams.template set<0>(std::move(inputBuffer), nullptr);
        mParams.template set<1>(LongT::type(0), nullptr);
        mParams.template set<2>(LongT::type(-1), nullptr);
        mParams.template set<3>(LongT::type(0), nullptr);
        mParams.template set<4>(LongT::type(-1), nullptr);
        mParams.template set<5>(std::move(harmOutputBuffer), nullptr);
        mParams.template set<6>(std::move(percOutputBuffer), nullptr);
        mParams.template set<7>(nullptr, nullptr);
        mParams.template set<8>(LongRuntimeMaxParam(harmFilterSize, harmFilterSize), nullptr);
        mParams.template set<9>(LongRuntimeMaxParam(percFilterSize, percFilterSize), nullptr);
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

//        BufferAdaptor::ReadAccess reader(outputBuffer.get());
//        if(!reader.exists() || !reader.valid()) return false;
//        auto view = reader.samps(0);
//
//        int markerCount = GetNumTakeMarkers(take);
//        for (int i = markerCount - 1; i >= 0; i--) {
//            DeleteTakeMarker(take, i);
//        }
//
//        const char* markerLabel = "";
//
//        for (fluid::index i = 0; i < view.size(); i++) {
//            if (view(i) > 0) {
//                double markerTimeInSeconds = static_cast<double>(view(i)) / sampleRate;
//                double markerPositionInTake = markerTimeInSeconds / playrate;
//                SetTakeMarker(take, -1, markerLabel, &markerPositionInTake, nullptr);
//            }
//        }
        return true;
    }
};

HPSSAlgorithm::HPSSAlgorithm(ReacomaExtension* apiProvider)
    : IAlgorithm(apiProvider),
      mImpl(std::make_unique<Impl>()) {}

HPSSAlgorithm::~HPSSAlgorithm() = default;

bool HPSSAlgorithm::ProcessItem(MediaItem* item) {
    return mImpl->processItemImpl(item, mApiProvider, mBaseParamIdx);
}

const char* HPSSAlgorithm::GetName() const {
    return "Harmonic Percussive Source Separation";
}

void HPSSAlgorithm::RegisterParameters() {
    mBaseParamIdx = mApiProvider->NParams();
    mImpl->registerParametersImpl(mApiProvider, mBaseParamIdx);
}

int HPSSAlgorithm::GetNumAlgorithmParams() const {
    return kNumParams;
}
