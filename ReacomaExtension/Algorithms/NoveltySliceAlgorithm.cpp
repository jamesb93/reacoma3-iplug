#include "NoveltySliceAlgorithm.h"
#include "IPlugParameter.h"
#include "ReacomaExtension.h"

NoveltySliceAlgorithm::NoveltySliceAlgorithm(ReacomaExtension *apiProvider)
    : SlicingAlgorithm<NRTThreadingNoveltySliceClient>(apiProvider) {}

NoveltySliceAlgorithm::~NoveltySliceAlgorithm() = default;

std::vector<ParameterDescriptor>
NoveltySliceAlgorithm::GetParamDescriptors() const {
    std::vector<ParameterDescriptor> descriptors;
    descriptors.push_back(
        {ParameterDescriptor::Double, "Threshold", 0.5, 0.0, 1.0, 0.01});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Kernel Size", 3.0, 3.0, 101.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Filter Size", 1.0, 1.0, 31.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Minimum Slice Length", 2.0, 2.0, 100.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Window Size", 1024.0, 2.0, 65536.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Hop Size", 512.0, 2.0, 65536.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "FFT Size", 1024.0, 2.0, 65536.0});
    descriptors.push_back(
        {ParameterDescriptor::Enum,
         "Algorithm",
         static_cast<double>(kSpectrum),
         0.0,
         static_cast<double>(kNumAlgorithmOptions - 1),
         0.0,
         {"Spectrum", "MFCC", "Chroma", "Pitch", "Loudness"}});
    return descriptors;
}

bool NoveltySliceAlgorithm::DoProcess(InputBufferT::type &sourceBuffer,
                                      int numChannels, int frameCount,
                                      int sampleRate) {
    int estimatedSlices = std::max(1, static_cast<int>(frameCount / 1024.0));
    auto outBuffer =
        std::make_shared<MemoryBufferAdaptor>(1, estimatedSlices, sampleRate);
    auto slicesOutputBuffer = fluid::client::BufferT::type(outBuffer);

    auto threshold = GetParamValue(kThreshold);
    auto kernelsize = GetParamValue(kKernelSize);
    auto filtersize = GetParamValue(kFilterSize);
    auto minslicelength = GetParamValue(kMinSliceLength);
    auto windowSize = GetParamValue(kWindowSize);
    auto hopSize = GetParamValue(kHopSize);
    auto fftSize = GetParamValue(kFFTSize);
    auto algorithm = GetParamValue(kAlgorithm);

    if (static_cast<int>(kernelsize) % 2 == 0)
        kernelsize += 1;
    if (static_cast<int>(filtersize) % 2 == 0)
        filtersize += 1;

    // FluCoMa Client Parameter Indices
    constexpr int kFlucomaInputAudio = 0;
    constexpr int kFlucomaStats = 1;
    constexpr int kFlucomaStartFrame = 2;
    constexpr int kFlucomaNumFrames = 3;
    constexpr int kFlucomaStartChan = 4;
    constexpr int kFlucomaNumChans = 5; // Used for slices output in this client
    constexpr int kFlucomaAlgorithm = 6;
    constexpr int kFlucomaKernelSize = 7;
    constexpr int kFlucomaThreshold = 8;
    constexpr int kFlucomaFilterSize = 9;
    constexpr int kFlucomaMinSliceLength = 10;
    constexpr int kFlucomaFFTParams = 11;

    mParams.template set<kFlucomaInputAudio>(std::move(sourceBuffer), nullptr);
    mParams.template set<kFlucomaStats>(std::move(LongT::type(0)), nullptr);
    mParams.template set<kFlucomaStartFrame>(std::move(LongT::type(-1)),
                                             nullptr);
    mParams.template set<kFlucomaNumFrames>(std::move(LongT::type(0)), nullptr);
    mParams.template set<kFlucomaStartChan>(std::move(LongT::type(-1)),
                                            nullptr);
    mParams.template set<kFlucomaNumChans>(std::move(slicesOutputBuffer),
                                           nullptr);
    mParams.template set<kFlucomaAlgorithm>(std::move(LongT::type(algorithm)),
                                            nullptr);
    mParams.template set<kFlucomaKernelSize>(
        std::move(LongRuntimeMaxParam(kernelsize, kernelsize)), nullptr);
    mParams.template set<kFlucomaThreshold>(std::move(FloatT::type(threshold)),
                                            nullptr);
    mParams.template set<kFlucomaFilterSize>(
        std::move(LongRuntimeMaxParam(filtersize, filtersize)), nullptr);
    mParams.template set<kFlucomaMinSliceLength>(
        std::move(LongT::type(minslicelength)), nullptr);
    mParams.template set<kFlucomaFFTParams>(
        std::move(fluid::client::FFTParams(windowSize, hopSize, fftSize,
                                           std::max(windowSize, fftSize))),
        nullptr);

    mClient = NRTThreadingNoveltySliceClient(mParams, mContext);
    mClient.setSynchronous(false);
    mClient.enqueue(mParams);
    Result result = mClient.process();

    return result.ok();
}

const char *NoveltySliceAlgorithm::GetName() const { return "Novelty Slice"; }

int NoveltySliceAlgorithm::GetNumAlgorithmParams() const { return kNumParams; }

std::unique_ptr<IAlgorithm> NoveltySliceAlgorithm::CreateNew() const {
    return std::make_unique<NoveltySliceAlgorithm>(mApiProvider);
}

BufferT::type &NoveltySliceAlgorithm::GetSlicesBuffer() {
    return mParams.template get<5>();
}