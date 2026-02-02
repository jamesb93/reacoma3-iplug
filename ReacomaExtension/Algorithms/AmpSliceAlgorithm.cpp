#include "AmpSliceAlgorithm.h"
#include "flucoma/clients/common/ParameterTypes.hpp"
#include "flucoma/clients/rt/AmpSliceClient.hpp"

AmpSliceAlgorithm::AmpSliceAlgorithm(IParameterProvider *apiProvider)
    : SlicingAlgorithm<NRTThreadedAmpSliceClient>(apiProvider) {}

AmpSliceAlgorithm::~AmpSliceAlgorithm() = default;

std::vector<ParameterDescriptor>
AmpSliceAlgorithm::GetParamDescriptors() const {
    std::vector<ParameterDescriptor> descriptors;
    descriptors.push_back({ParameterDescriptor::Int,
                           "Fast Ramp Up Length (samples)", 3.0, 1.0, 88200.0});
    descriptors.push_back({ParameterDescriptor::Int,
                           "Fast Ramp Down Length (samples)", 383.0, 1.0,
                           88200.0});
    descriptors.push_back({ParameterDescriptor::Int,
                           "Slow Ramp Up Length (samples)", 2205.0, 1.0,
                           88200.0});
    descriptors.push_back({ParameterDescriptor::Int,
                           "Slow Ramp Down Length (samples)", 2205.0, 1.0,
                           88200.0});
    descriptors.push_back({ParameterDescriptor::Double, "On Threshold (dB)",
                           19.0, -144.0, 144.0});
    descriptors.push_back({ParameterDescriptor::Double, "Off Threshold (dB)",
                           8.0, -144.0, 144.0});
    descriptors.push_back({ParameterDescriptor::Double, "Floor value (dB)",
                           -70.0, -144.0, 144.0});
    descriptors.push_back({ParameterDescriptor::Int,
                           "Minimum Slice Length (samples)", 1323.0, 1.0,
                           88200.0});
    descriptors.push_back({ParameterDescriptor::Double,
                           "High-Pass Filter Cutoff (Hz)", 2000.0, 0.0,
                           10000.0});
    return descriptors;
}

bool AmpSliceAlgorithm::DoProcess(InputBufferT::type &sourceBuffer,
                                  int numChannels, int frameCount,
                                  int sampleRate) {
    int estimatedSlices = std::max(1, static_cast<int>(frameCount / 1024.0));
    auto outBuffer =
        std::make_shared<MemoryBufferAdaptor>(1, estimatedSlices, sampleRate);
    auto slicesOutputBuffer = fluid::client::BufferT::type(outBuffer);

    auto fastRampUpTime = GetParamValue(kFastRampUpTime);
    auto fastRampDownTime = GetParamValue(kFastRampDownTime);
    auto slowRampUpTime = GetParamValue(kSlowRampUpTime);
    auto slowRampDownTime = GetParamValue(kSlowRampDownTime);
    auto onThreshold = GetParamValue(kOnThreshold);
    auto offThreshold = GetParamValue(kOffThreshold);
    auto floorValue = GetParamValue(kSilenceThreshold);
    auto debounceTime = GetParamValue(kDebounce);
    auto hiPassFreq = GetParamValue(kHiPassFreq);

    // FluCoMa Client Parameter Indices
    constexpr int kFlucomaInputAudio = 0;
    constexpr int kFlucomaStats = 1;
    constexpr int kFlucomaStartFrame = 2;
    constexpr int kFlucomaNumFrames = 3;
    constexpr int kFlucomaStartChan = 4;
    constexpr int kFlucomaNumChans = 5; // Slices output
    constexpr int kFlucomaFastRampUp = 6;
    constexpr int kFlucomaFastRampDown = 7;
    constexpr int kFlucomaSlowRampUp = 8;
    constexpr int kFlucomaSlowRampDown = 9;
    constexpr int kFlucomaOnThreshold = 10;
    constexpr int kFlucomaOffThreshold = 11;
    constexpr int kFlucomaFloor = 12;
    constexpr int kFlucomaMinSliceLength = 13;
    constexpr int kFlucomaHiPassFreq = 14;

    mParams.template set<kFlucomaInputAudio>(std::move(sourceBuffer), nullptr);
    mParams.template set<kFlucomaStats>(std::move(LongT::type(0)), nullptr);
    mParams.template set<kFlucomaStartFrame>(std::move(LongT::type(-1)),
                                             nullptr);
    mParams.template set<kFlucomaNumFrames>(std::move(LongT::type(0)), nullptr);
    mParams.template set<kFlucomaStartChan>(std::move(LongT::type(-1)),
                                            nullptr);
    mParams.template set<kFlucomaNumChans>(std::move(slicesOutputBuffer),
                                           nullptr);
    mParams.template set<kFlucomaFastRampUp>(
        std::move(LongT::type(fastRampUpTime)), nullptr);
    mParams.template set<kFlucomaFastRampDown>(
        std::move(LongT::type(fastRampDownTime)), nullptr);
    mParams.template set<kFlucomaSlowRampUp>(
        std::move(LongT::type(slowRampUpTime)), nullptr);
    mParams.template set<kFlucomaSlowRampDown>(
        std::move(LongT::type(slowRampDownTime)), nullptr);
    mParams.template set<kFlucomaOnThreshold>(
        std::move(FloatT::type(onThreshold)), nullptr);
    mParams.template set<kFlucomaOffThreshold>(
        std::move(FloatT::type(offThreshold)), nullptr);
    mParams.template set<kFlucomaFloor>(std::move(FloatT::type(floorValue)),
                                        nullptr);
    mParams.template set<kFlucomaMinSliceLength>(
        std::move(LongT::type(debounceTime)), nullptr);
    mParams.template set<kFlucomaHiPassFreq>(
        std::move(FloatT::type(hiPassFreq)), nullptr);

    mClient = NRTThreadedAmpSliceClient(mParams, mContext);
    mClient.setSynchronous(false);
    mClient.enqueue(mParams);
    Result result = mClient.process();
    return result.ok();
}

const char *AmpSliceAlgorithm::GetName() const { return "Amp Slice"; }

int AmpSliceAlgorithm::GetNumAlgorithmParams() const { return kNumParams; }

std::unique_ptr<IAlgorithm> AmpSliceAlgorithm::CreateNew() const {
    return std::make_unique<AmpSliceAlgorithm>(mApiProvider);
}

BufferT::type &AmpSliceAlgorithm::GetSlicesBuffer() {
    return mParams.template get<5>();
}