#include "NMFAlgorithm.h"
#include "IPlugParameter.h"
#include "ReacomaExtension.h"

NMFAlgorithm::NMFAlgorithm(ReacomaExtension *apiProvider)
    : AudioOutputAlgorithm<NRTThreadedNMFClient>(apiProvider) {}

NMFAlgorithm::~NMFAlgorithm() = default;

std::vector<ParameterDescriptor> NMFAlgorithm::GetParamDescriptors() const {
    std::vector<ParameterDescriptor> descriptors;
    descriptors.push_back(
        {ParameterDescriptor::Int, "Number of Components", 2.0, 2.0, 10.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Number of Iterations", 100.0, 1.0, 1000.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Window Size", 1024.0, 2.0, 65536.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Hop Size", 512.0, 2.0, 65536.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "FFT Size", 1024.0, 2.0, 65536.0});
    return descriptors;
}

bool NMFAlgorithm::DoProcess(InputBufferT::type &sourceBuffer, int numChannels,
                             int frameCount, int sampleRate) {
    auto componentsParam = GetParamValue(kComponents);
    auto iterationsParam = GetParamValue(kIterations);
    auto windowSize = GetParamValue(kWindowSize);
    auto hopSize = GetParamValue(kHopSize);
    auto fftSize = GetParamValue(kFFTSize);

    auto resynthMemoryBuffer = std::make_shared<MemoryBufferAdaptor>(
        numChannels * static_cast<int>(componentsParam), frameCount,
        sampleRate);
    auto resynthOutputBuffer =
        fluid::client::BufferT::type(resynthMemoryBuffer);

    // FluCoMa Client Parameter Indices
    constexpr int kFlucomaInputAudio = 0;
    constexpr int kFlucomaStartChan = 1;
    constexpr int kFlucomaNumChans = 2;
    constexpr int kFlucomaStartFrame = 3;
    constexpr int kFlucomaNumFrames = 4;
    constexpr int kFlucomaResynth = 5;
    constexpr int kFlucomaResynthMode = 6;
    constexpr int kFlucomaBases = 7;
    constexpr int kFlucomaBasesMode = 8;
    constexpr int kFlucomaActivations = 9;
    constexpr int kFlucomaActivationsMode = 10;
    constexpr int kFlucomaComponents = 11;
    constexpr int kFlucomaIterations = 12;
    constexpr int kFlucomaFFTParams = 13;

    mParams.template set<kFlucomaInputAudio>(std::move(sourceBuffer), nullptr);
    mParams.template set<kFlucomaStartChan>(std::move(LongT::type(0)), nullptr);
    mParams.template set<kFlucomaNumChans>(std::move(LongT::type(-1)), nullptr);
    mParams.template set<kFlucomaStartFrame>(std::move(LongT::type(0)),
                                             nullptr);
    mParams.template set<kFlucomaNumFrames>(std::move(LongT::type(-1)),
                                            nullptr);
    mParams.template set<kFlucomaResynth>(std::move(resynthOutputBuffer),
                                          nullptr);
    mParams.template set<kFlucomaResynthMode>(std::move(LongT::type(1)),
                                              nullptr);
    mParams.template set<kFlucomaBases>(nullptr, nullptr);
    mParams.template set<kFlucomaBasesMode>(std::move(LongT::type(0)), nullptr);
    mParams.template set<kFlucomaActivations>(nullptr, nullptr);
    mParams.template set<kFlucomaActivationsMode>(std::move(LongT::type(0)),
                                                  nullptr);
    mParams.template set<kFlucomaComponents>(
        std::move(LongT::type(componentsParam)), nullptr);
    mParams.template set<kFlucomaIterations>(
        std::move(LongT::type(iterationsParam)), nullptr);
    mParams.template set<kFlucomaFFTParams>(
        std::move(fluid::client::FFTParams(windowSize, hopSize, fftSize,
                                           std::max(windowSize, fftSize))),
        nullptr);

    mClient = NRTThreadedNMFClient(mParams, mContext);
    mClient.setSynchronous(false);
    mClient.enqueue(mParams);
    Result result = mClient.process();

    return result.ok();
}

bool NMFAlgorithm::HandleResults(MediaItem *item, MediaItem_Take *take,
                                 int numChannels, int sampleRate) {
    auto resynthOutputBuffer =
        mParams.template get<5>(); // Get the buffer we created in DoProcess
    AddOutputToTake(item, resynthOutputBuffer, sampleRate, "nmf");
    return true;
}

const char *NMFAlgorithm::GetName() const {
    return "Non-negative Matrix Factorisation";
}

int NMFAlgorithm::GetNumAlgorithmParams() const { return kNumParams; }

std::unique_ptr<IAlgorithm> NMFAlgorithm::CreateNew() const {
    return std::make_unique<NMFAlgorithm>(mApiProvider);
}
