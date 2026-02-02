#include "HPSSAlgorithm.h"

HPSSAlgorithm::HPSSAlgorithm(IParameterProvider *apiProvider)
    : AudioOutputAlgorithm<NRTThreadedHPSSClient>(apiProvider) {}

HPSSAlgorithm::~HPSSAlgorithm() = default;

std::vector<ParameterDescriptor> HPSSAlgorithm::GetParamDescriptors() const {
    std::vector<ParameterDescriptor> descriptors;
    descriptors.push_back(
        {ParameterDescriptor::Int, "Harmonic Filter Size", 17.0, 3.0, 101.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Percussive Filter Size", 31.0, 3.0, 101.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Window Size", 1024.0, 2.0, 65536.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Hop Size", 512.0, 2.0, 65536.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "FFT Size", 1024.0, 2.0, 65536.0});
    return descriptors;
}

bool HPSSAlgorithm::DoProcess(InputBufferT::type &sourceBuffer, int numChannels,
                              int frameCount, int sampleRate) {
    auto harmFilterSizeParam = GetParamValue(kHarmFilterSize);
    auto percFilterSizeParam = GetParamValue(kPercFilterSize);
    auto windowSize = GetParamValue(kWindowSize);
    auto hopSize = GetParamValue(kHopSize);
    auto fftSize = GetParamValue(kFFTSize);

    auto harmMemoryBuffer =
        std::make_shared<MemoryBufferAdaptor>(1, frameCount, sampleRate);
    auto percMemoryBuffer =
        std::make_shared<MemoryBufferAdaptor>(1, frameCount, sampleRate);
    auto harmOutputBuffer = fluid::client::BufferT::type(harmMemoryBuffer);
    auto percOutputBuffer = fluid::client::BufferT::type(percMemoryBuffer);

    if (static_cast<int>(harmFilterSizeParam) % 2 == 0)
        harmFilterSizeParam += 1;
    if (static_cast<int>(percFilterSizeParam) % 2 == 0)
        percFilterSizeParam += 1;

    // FluCoMa Client Parameter Indices
    constexpr int kFlucomaInputAudio = 0;
    constexpr int kFlucomaStartChan = 1;
    constexpr int kFlucomaNumChans = 2;
    constexpr int kFlucomaStartFrame = 3;
    constexpr int kFlucomaNumFrames = 4;
    constexpr int kFlucomaHarmonic = 5;
    constexpr int kFlucomaPercussive = 6;
    constexpr int kFlucomaResidual = 7;
    constexpr int kFlucomaHarmFilterSize = 8;
    constexpr int kFlucomaPercFilterSize = 9;
    constexpr int kFlucomaMaskMode = 10;
    constexpr int kFlucomaHarmBins = 11;
    constexpr int kFlucomaPercBins = 12;
    constexpr int kFlucomaFFTParams = 13;

    mParams.template set<kFlucomaInputAudio>(std::move(sourceBuffer), nullptr);
    mParams.template set<kFlucomaStartChan>(std::move(LongT::type(0)), nullptr);
    mParams.template set<kFlucomaNumChans>(std::move(LongT::type(-1)), nullptr);
    mParams.template set<kFlucomaStartFrame>(std::move(LongT::type(0)),
                                             nullptr);
    mParams.template set<kFlucomaNumFrames>(std::move(LongT::type(-1)),
                                            nullptr);
    mParams.template set<kFlucomaHarmonic>(std::move(harmOutputBuffer),
                                           nullptr);
    mParams.template set<kFlucomaPercussive>(std::move(percOutputBuffer),
                                             nullptr);
    mParams.template set<kFlucomaResidual>(nullptr, nullptr);
    mParams.template set<kFlucomaHarmFilterSize>(
        std::move(
            LongRuntimeMaxParam(harmFilterSizeParam, harmFilterSizeParam)),
        nullptr);
    mParams.template set<kFlucomaPercFilterSize>(
        std::move(
            LongRuntimeMaxParam(percFilterSizeParam, percFilterSizeParam)),
        nullptr);
    mParams.template set<kFlucomaMaskMode>(std::move(LongT::type(0)), nullptr);
    mParams.template set<kFlucomaHarmBins>(
        std::move(FloatPairsArrayT::type(0, 1, 1, 1)), nullptr);
    mParams.template set<kFlucomaPercBins>(
        std::move(FloatPairsArrayT::type(1, 0, 1, 1)), nullptr);
    mParams.template set<kFlucomaFFTParams>(
        std::move(fluid::client::FFTParams(windowSize, hopSize, fftSize,
                                           std::max(windowSize, fftSize))),
        nullptr);

    mClient = NRTThreadedHPSSClient(mParams, mContext);
    mClient.setSynchronous(false);
    mClient.enqueue(mParams);
    Result result = mClient.process();

    if (!result.ok()) {
        ShowConsoleMsg("HPSS processing failed.\n");
        return false;
    }
    return true;
}

bool HPSSAlgorithm::HandleResults(MediaItem *item, MediaItem_Take *take,
                                  int numChannels, int sampleRate) {
    auto harmOutputBuffer = mParams.template get<5>();
    auto percOutputBuffer = mParams.template get<6>();

    AddOutputToTake(item, harmOutputBuffer, sampleRate, "harmonic");
    AddOutputToTake(item, percOutputBuffer, sampleRate, "percussive");
    return true;
}

const char *HPSSAlgorithm::GetName() const {
    return "Harmonic Percussive Source Separation";
}

int HPSSAlgorithm::GetNumAlgorithmParams() const { return kNumParams; }

std::unique_ptr<IAlgorithm> HPSSAlgorithm::CreateNew() const {
    return std::make_unique<HPSSAlgorithm>(mApiProvider);
}
