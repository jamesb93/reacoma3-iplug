#include "OnsetSliceAlgorithm.h"

OnsetSliceAlgorithm::OnsetSliceAlgorithm(IParameterProvider *apiProvider)
    : FlucomaAlgorithm<NRTThreadingOnsetSliceClient>(apiProvider) {}

OnsetSliceAlgorithm::~OnsetSliceAlgorithm() = default;

std::vector<ParameterDescriptor>
OnsetSliceAlgorithm::GetParamDescriptors() const {
    std::vector<ParameterDescriptor> descriptors;
    descriptors.push_back(
        {ParameterDescriptor::Enum,
         "Metric",
         0.0,
         0.0,
         9.0,
         0.0,
         {"Energy", "HFC", "SpectralDiff", "ComplexDomain", "PhaseDev",
          "WPhaseDev", "SpectralFlux", "ModifiedKL", "ISDiff", "Cosine"}});
    descriptors.push_back(
        {ParameterDescriptor::Double, "Threshold", 0.5, 0.0, 1.0, 0.01});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Min Length", 2.0, 1.0, 1000.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Filter Size", 5.0, 1.0, 101.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Frame Delta", 0.0, 0.0, 10.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Window Size", 1024.0, 2.0, 65536.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Hop Size", 512.0, 2.0, 65536.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "FFT Size", 1024.0, 2.0, 65536.0});
    return descriptors;
}

bool OnsetSliceAlgorithm::DoProcess(InputBufferT::type &sourceBuffer,
                                    int numChannels, int frameCount,
                                    int sampleRate) {
    int estimatedSlices = std::max(1, static_cast<int>(frameCount / 1024.0));
    auto outBuffer =
        std::make_shared<MemoryBufferAdaptor>(1, estimatedSlices, sampleRate);
    auto slicesOutputBuffer = fluid::client::BufferT::type(outBuffer);

    auto metric = GetParamValue(kMetric);
    auto threshold = GetParamValue(kThreshold);
    auto filterSize = GetParamValue(kFilterSize);
    auto frameDelta = GetParamValue(kFrameDelta);
    auto minLength = GetParamValue(kMinSliceLength);
    auto windowSize = GetParamValue(kWindowSize);
    auto hopSize = GetParamValue(kHopSize);
    auto fftSize = GetParamValue(kFFTSize);

    if (static_cast<int>(filterSize) % 2 == 0)
        filterSize += 1;

    // FluCoMa Client Parameter Indices
    constexpr int kFlucomaInputAudio = 0;
    constexpr int kFlucomaStartChan = 1;
    constexpr int kFlucomaNumChans = 2;
    constexpr int kFlucomaStartFrame = 3;
    constexpr int kFlucomaNumFrames = 4;
    constexpr int kFlucomaOnsets = 5;
    constexpr int kFlucomaMetric = 6;
    constexpr int kFlucomaThreshold = 7;
    constexpr int kFlucomaMinSliceLength = 8;
    constexpr int kFlucomaFilterSize = 9;
    constexpr int kFlucomaFrameDelta = 10;
    constexpr int kFlucomaFFTParams = 11;

    mParams.template set<kFlucomaInputAudio>(std::move(sourceBuffer), nullptr);
    mParams.template set<kFlucomaStartChan>(std::move(LongT::type(0)), nullptr);
    mParams.template set<kFlucomaNumChans>(std::move(LongT::type(-1)), nullptr);
    mParams.template set<kFlucomaStartFrame>(std::move(LongT::type(0)),
                                             nullptr);
    mParams.template set<kFlucomaNumFrames>(std::move(LongT::type(-1)),
                                            nullptr);
    mParams.template set<kFlucomaOnsets>(std::move(slicesOutputBuffer),
                                         nullptr);
    mParams.template set<kFlucomaMetric>(std::move(LongT::type(metric)),
                                         nullptr);
    mParams.template set<kFlucomaThreshold>(std::move(FloatT::type(threshold)),
                                            nullptr);
    mParams.template set<kFlucomaMinSliceLength>(
        std::move(LongT::type(minLength)), nullptr);
    mParams.template set<kFlucomaFilterSize>(
        std::move(LongRuntimeMaxParam(filterSize, filterSize)), nullptr);
    mParams.template set<kFlucomaFrameDelta>(std::move(LongT::type(frameDelta)),
                                             nullptr);
    mParams.template set<kFlucomaFFTParams>(
        std::move(fluid::client::FFTParams(windowSize, hopSize, fftSize,
                                           std::max(windowSize, fftSize))),
        nullptr);

    mClient = NRTThreadingOnsetSliceClient(mParams, mContext);
    mClient.setSynchronous(false);
    mClient.enqueue(mParams);
    Result result = mClient.process();
    return result.ok();
}

bool OnsetSliceAlgorithm::HandleResults(MediaItem *item, MediaItem_Take *take,
                                        int numChannels, int sampleRate) {
    auto processedSlicesBuffer = mParams.template get<5>();
    BufferAdaptor::ReadAccess reader(processedSlicesBuffer.get());

    if (!reader.exists() || !reader.valid())
        return false;

    int markerCount = GetNumTakeMarkers(take);
    for (int i = markerCount - 1; i >= 0; i--) {
        DeleteTakeMarker(take, i);
    }

    double itemLength = GetMediaItemInfo_Value(item, "D_LENGTH");
    auto view = reader.samps(0);
    for (fluid::index i = 0; i < view.size(); i++) {
        if (view(i) > 0) {
            double markerTimeInSeconds =
                static_cast<double>(view(i)) / sampleRate;
            if (markerTimeInSeconds < itemLength) {
                SetTakeMarker(take, -1, "", &markerTimeInSeconds, nullptr);
            }
        }
    }
    return true;
}

const char *OnsetSliceAlgorithm::GetName() const { return "Onset Slice"; }

int OnsetSliceAlgorithm::GetNumAlgorithmParams() const { return kNumParams; }

std::unique_ptr<IAlgorithm> OnsetSliceAlgorithm::CreateNew() const {
    return std::make_unique<OnsetSliceAlgorithm>(mApiProvider);
}
