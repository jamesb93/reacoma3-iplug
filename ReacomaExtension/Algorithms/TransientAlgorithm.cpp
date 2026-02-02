#include "TransientAlgorithm.h"

TransientAlgorithm::TransientAlgorithm(IParameterProvider *apiProvider)
    : AudioOutputAlgorithm<NRTThreadedTransientsClient>(apiProvider) {}

TransientAlgorithm::~TransientAlgorithm() = default;

std::vector<ParameterDescriptor>
TransientAlgorithm::GetParamDescriptors() const {
    std::vector<ParameterDescriptor> descriptors;
    descriptors.push_back(
        {ParameterDescriptor::Int, "Order", 100.0, 1.0, 1000.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Block Size", 256.0, 2.0, 4096.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Padding", 128.0, 0.0, 2048.0});
    descriptors.push_back(
        {ParameterDescriptor::Double, "Skew", 0.0, -10.0, 10.0, 0.1});
    descriptors.push_back({ParameterDescriptor::Double, "Forward Threshold",
                           2.0, 0.0, 10.0, 0.01});
    descriptors.push_back({ParameterDescriptor::Double, "Backward Threshold",
                           1.1, 0.0, 10.0, 0.01});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Window Size", 14.0, 0.0, 100.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Clump Length", 25.0, 0.0, 100.0});
    return descriptors;
}

bool TransientAlgorithm::DoProcess(InputBufferT::type &sourceBuffer,
                                   int numChannels, int frameCount,
                                   int sampleRate) {
    auto order = GetParamValue(kOrder);
    auto blockSize = GetParamValue(kBlockSize);
    auto padding = GetParamValue(kPadding);
    auto skew = GetParamValue(kSkew);
    auto fwd = GetParamValue(kThreshFwd);
    auto bwd = GetParamValue(kThreshBack);
    auto winSize = GetParamValue(kWinSize);
    auto clumpLength = GetParamValue(kClump);

    auto transMemoryBuffer = std::make_shared<MemoryBufferAdaptor>(
        numChannels, frameCount, sampleRate);
    auto resMemoryBuffer = std::make_shared<MemoryBufferAdaptor>(
        numChannels, frameCount, sampleRate);
    auto transOutputBuffer = fluid::client::BufferT::type(transMemoryBuffer);
    auto resOutputBuffer = fluid::client::BufferT::type(resMemoryBuffer);

    // FluCoMa Client Parameter Indices
    constexpr int kFlucomaInputAudio = 0;
    constexpr int kFlucomaStartChan = 1;
    constexpr int kFlucomaNumChans = 2;
    constexpr int kFlucomaStartFrame = 3;
    constexpr int kFlucomaNumFrames = 4;
    constexpr int kFlucomaTransients = 5;
    constexpr int kFlucomaResidual = 6;
    constexpr int kFlucomaOrder = 7;
    constexpr int kFlucomaBlockSize = 8;
    constexpr int kFlucomaPadding = 9;
    constexpr int kFlucomaSkew = 10;
    constexpr int kFlucomaThreshFwd = 11;
    constexpr int kFlucomaThreshBack = 12;
    constexpr int kFlucomaWinSize = 13;
    constexpr int kFlucomaClump = 14;

    mParams.template set<kFlucomaInputAudio>(std::move(sourceBuffer), nullptr);
    mParams.template set<kFlucomaStartChan>(std::move(LongT::type(0)), nullptr);
    mParams.template set<kFlucomaNumChans>(std::move(LongT::type(-1)), nullptr);
    mParams.template set<kFlucomaStartFrame>(std::move(LongT::type(0)),
                                             nullptr);
    mParams.template set<kFlucomaNumFrames>(std::move(LongT::type(-1)),
                                            nullptr);
    mParams.template set<kFlucomaTransients>(std::move(transOutputBuffer),
                                             nullptr); // transients
    mParams.template set<kFlucomaResidual>(std::move(resOutputBuffer),
                                           nullptr); // residual
    mParams.template set<kFlucomaOrder>(std::move(LongT::type(order)), nullptr);
    mParams.template set<kFlucomaBlockSize>(std::move(LongT::type(blockSize)),
                                            nullptr);
    mParams.template set<kFlucomaPadding>(std::move(LongT::type(padding)),
                                          nullptr);
    mParams.template set<kFlucomaSkew>(std::move(FloatT::type(skew)), nullptr);
    mParams.template set<kFlucomaThreshFwd>(std::move(FloatT::type(fwd)),
                                            nullptr);
    mParams.template set<kFlucomaThreshBack>(std::move(FloatT::type(bwd)),
                                             nullptr);
    mParams.template set<kFlucomaWinSize>(std::move(LongT::type(winSize)),
                                          nullptr);
    mParams.template set<kFlucomaClump>(std::move(LongT::type(clumpLength)),
                                        nullptr);

    mClient = NRTThreadedTransientsClient(mParams, mContext);
    mClient.setSynchronous(false);
    mClient.enqueue(mParams);
    Result result = mClient.process();

    return result.ok();
}

bool TransientAlgorithm::HandleResults(MediaItem *item, MediaItem_Take *take,
                                       int numChannels, int sampleRate) {
    auto transOutputBuffer = mParams.template get<5>();
    auto resOutputBuffer = mParams.template get<6>();

    AddOutputToTake(item, transOutputBuffer, sampleRate, "transients");
    AddOutputToTake(item, resOutputBuffer, sampleRate, "residual");
    return true;
}

const char *TransientAlgorithm::GetName() const {
    return "Transient Separation";
}

int TransientAlgorithm::GetNumAlgorithmParams() const { return kNumParams; }

std::unique_ptr<IAlgorithm> TransientAlgorithm::CreateNew() const {
    return std::make_unique<TransientAlgorithm>(mApiProvider);
}
