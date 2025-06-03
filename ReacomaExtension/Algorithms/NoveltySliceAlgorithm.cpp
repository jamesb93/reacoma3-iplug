#include "NoveltySliceAlgorithm.h"
#include "IPlugParameter.h"
#include "FlucomaAlgorithmBase.h"
#include "ReacomaExtension.h"

NoveltySliceAlgorithm::NoveltySliceAlgorithm(ReacomaExtension* apiProvider)
    : FlucomaAlgorithm<NRTThreadingNoveltySliceClient>(apiProvider) {}

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

bool NoveltySliceAlgorithm::DoProcess(InputBufferT::type& sourceBuffer, int numChannels, int frameCount, int sampleRate) {
    int estimatedSlices = std::max(1, static_cast<int>(frameCount / 1024.0));
    auto outBuffer = std::make_shared<MemoryBufferAdaptor>(1, estimatedSlices, sampleRate);
    auto slicesOutputBuffer = fluid::client::BufferT::type(outBuffer);

    auto threshold = mApiProvider->GetParam(mBaseParamIdx + NoveltySliceAlgorithm::kThreshold)->Value();
    auto filtersize = mApiProvider->GetParam(mBaseParamIdx + NoveltySliceAlgorithm::kFilterSize)->Value();
    auto kernelsize = mApiProvider->GetParam(mBaseParamIdx + NoveltySliceAlgorithm::kKernelSize)->Value();
    auto minslicelength = mApiProvider->GetParam(mBaseParamIdx + NoveltySliceAlgorithm::kMinSliceLength)->Value();
    auto algorithm = mApiProvider->GetParam(mBaseParamIdx + NoveltySliceAlgorithm::kAlgorithm)->Value();
    
    if (static_cast<int>(kernelsize) % 2 == 0) kernelsize +=1;
    if (static_cast<int>(filtersize) % 2 == 0) filtersize +=1;
    
    mParams.template set<0>(std::move(sourceBuffer), nullptr);
    mParams.template set<1>(LongT::type(0), nullptr);
    mParams.template set<2>(LongT::type(-1), nullptr);
    mParams.template set<3>(LongT::type(0), nullptr);
    mParams.template set<4>(LongT::type(-1), nullptr);
    mParams.template set<5>(std::move(slicesOutputBuffer), nullptr);
    mParams.template set<6>(LongT::type(algorithm), nullptr);
    mParams.template set<7>(LongRuntimeMaxParam(kernelsize, kernelsize), nullptr);
    mParams.template set<8>(FloatT::type(threshold), nullptr);
    mParams.template set<9>(LongRuntimeMaxParam(filtersize, filtersize), nullptr);
    mParams.template set<10>(LongT::type(minslicelength), nullptr);
    mParams.template set<11>(fluid::client::FFTParams(1024, -1, -1), nullptr); // fftsettings
    
    mClient = NRTThreadingNoveltySliceClient(mParams, mContext);
    mClient.setSynchronous(true);
    mClient.enqueue(mParams);
    Result result = mClient.process();

    return result.ok();
}

bool NoveltySliceAlgorithm::HandleResults(MediaItem* item, MediaItem_Take* take, int numChannels, int sampleRate) {
    auto processedSlicesBuffer = mParams.template get<5>();
    BufferAdaptor::ReadAccess reader(processedSlicesBuffer.get());

    if (!reader.exists() || !reader.valid()) return false;

    // Clear existing markers
    int markerCount = GetNumTakeMarkers(take);
    for (int i = markerCount - 1; i >= 0; i--) {
        DeleteTakeMarker(take, i);
    }
    
    double itemLength = GetMediaItemInfo_Value(item, "D_LENGTH");
    auto view = reader.samps(0);
    for (fluid::index i = 0; i < view.size(); i++) {
        if (view(i) > 0) {
            double markerTimeInSeconds = static_cast<double>(view(i)) / sampleRate;
            if (markerTimeInSeconds < itemLength) {
                SetTakeMarker(take, -1, "", &markerTimeInSeconds, nullptr);
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
