#include "AmpGateAlgorithm.h"

AmpGateAlgorithm::AmpGateAlgorithm(IParameterProvider *apiProvider)
    : FlucomaAlgorithm<NRTThreadedAmpGateClient>(apiProvider) {}

AmpGateAlgorithm::~AmpGateAlgorithm() = default;

std::vector<ParameterDescriptor> AmpGateAlgorithm::GetParamDescriptors() const {
    std::vector<ParameterDescriptor> descriptors;
    descriptors.push_back({ParameterDescriptor::Int, "Ramp Up Length (samples)",
                           10.0, 1.0, 88200.0});
    descriptors.push_back({ParameterDescriptor::Int,
                           "Ramp Down Length (samples)", 10.0, 1.0, 88200.0});
    descriptors.push_back({ParameterDescriptor::Double, "On Threshold (dB)",
                           -12.0, -144.0, 144.0, 0.1});
    descriptors.push_back({ParameterDescriptor::Double, "Off Threshold (dB)",
                           -24.0, -144.0, 144.0, 0.1});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Minimum Slice Length", 1.0, 1.0, 88200.0});
    descriptors.push_back({ParameterDescriptor::Int, "Minimum Silence Length",
                           1.0, 1.0, 88200.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Minimum Length Above", 1.0, 1.0, 88200.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Minimum Length Below", 1.0, 1.0, 88200.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Lookback", 0.0, 0.0, 88200.0});
    descriptors.push_back(
        {ParameterDescriptor::Int, "Lookahead", 0.0, 0.0, 88200.0});
    descriptors.push_back({ParameterDescriptor::Double,
                           "High-Pass Filter Cutoff (Hz)", 85.0, 0.0, 5000.0});
    return descriptors;
}

bool AmpGateAlgorithm::DoProcess(InputBufferT::type &sourceBuffer,
                                 int numChannels, int frameCount,
                                 int sampleRate) {
    int estimatedSlices = std::max(1, static_cast<int>(frameCount / 1024.0));
    auto outBuffer =
        std::make_shared<MemoryBufferAdaptor>(1, estimatedSlices, sampleRate);
    auto slicesOutputBuffer = fluid::client::BufferT::type(outBuffer);

    auto rampUpTime = GetParamValue(kRampUpTime);
    auto rampDownTime = GetParamValue(kRampDownTime);
    auto onThreshold = GetParamValue(kOnThreshold);
    auto offThreshold = GetParamValue(kOffThreshold);
    auto minEventDuration = GetParamValue(kMinEventDuration);
    auto minSilenceDuration = GetParamValue(kMinSilenceDuration);
    auto minTimeAboveThreshold = GetParamValue(kMinTimeAboveThreshold);
    auto minTimeBelowThreshold = GetParamValue(kMinTimeBelowThreshold);
    auto upwardLookupTime = GetParamValue(kUpwardLookupTime);
    auto downwardLookupTime = GetParamValue(kDownwardLookupTime);
    auto hiPassFreq = GetParamValue(kHiPassFreq);

    // FluCoMa Client Parameter Indices
    constexpr int kFlucomaInputAudio = 0;
    constexpr int kFlucomaStartChan = 1;
    constexpr int kFlucomaNumChans = 2;
    constexpr int kFlucomaStartFrame = 3;
    constexpr int kFlucomaNumFrames = 4;
    constexpr int kFlucomaOnsets = 5;
    constexpr int kFlucomaRampUp = 6;
    constexpr int kFlucomaRampDown = 7;
    constexpr int kFlucomaOnThreshold = 8;
    constexpr int kFlucomaOffThreshold = 9;
    constexpr int kFlucomaMinEventDuration = 10;
    constexpr int kFlucomaMinSilenceDuration = 11;
    constexpr int kFlucomaMinTimeAboveThreshold = 12;
    constexpr int kFlucomaMinTimeBelowThreshold = 13;
    constexpr int kFlucomaUpwardLookupTime = 14;
    constexpr int kFlucomaDownwardLookupTime = 15;
    constexpr int kFlucomaHiPassFreq = 16;

    mParams.template set<kFlucomaInputAudio>(std::move(sourceBuffer), nullptr);
    mParams.template set<kFlucomaStartChan>(std::move(LongT::type(0)), nullptr);
    mParams.template set<kFlucomaNumChans>(std::move(LongT::type(-1)), nullptr);
    mParams.template set<kFlucomaStartFrame>(std::move(LongT::type(0)),
                                             nullptr);
    mParams.template set<kFlucomaNumFrames>(std::move(LongT::type(-1)),
                                            nullptr);
    mParams.template set<kFlucomaOnsets>(std::move(slicesOutputBuffer),
                                         nullptr);
    mParams.template set<kFlucomaRampUp>(std::move(LongT::type(rampUpTime)),
                                         nullptr);
    mParams.template set<kFlucomaRampDown>(std::move(LongT::type(rampDownTime)),
                                           nullptr);
    mParams.template set<kFlucomaOnThreshold>(
        std::move(FloatT::type(onThreshold)), nullptr);
    mParams.template set<kFlucomaOffThreshold>(
        std::move(FloatT::type(offThreshold)), nullptr);
    mParams.template set<kFlucomaMinEventDuration>(
        std::move(LongT::type(minEventDuration)), nullptr);
    mParams.template set<kFlucomaMinSilenceDuration>(
        std::move(LongT::type(minSilenceDuration)), nullptr);
    mParams.template set<kFlucomaMinTimeAboveThreshold>(
        std::move(LongT::type(minTimeAboveThreshold)), nullptr);
    mParams.template set<kFlucomaMinTimeBelowThreshold>(
        std::move(LongT::type(minTimeBelowThreshold)), nullptr);
    mParams.template set<kFlucomaUpwardLookupTime>(
        std::move(LongT::type(upwardLookupTime)), nullptr);
    mParams.template set<kFlucomaDownwardLookupTime>(
        std::move(LongT::type(downwardLookupTime)), nullptr);
    mParams.template set<kFlucomaHiPassFreq>(
        std::move(FloatT::type(hiPassFreq)), nullptr);

    mClient = NRTThreadedAmpGateClient(mParams, mContext);
    mClient.setSynchronous(false);
    mClient.enqueue(mParams);
    Result result = mClient.process();
    return result.ok();
}

bool AmpGateAlgorithm::HandleResults(MediaItem *item, MediaItem_Take *take,
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
    auto onsetView = reader.samps(0);
    for (fluid::index i = 0; i < onsetView.size(); i++) {
        if (onsetView(i) > 0) {
            double markerTimeInSeconds =
                static_cast<double>(onsetView(i)) / sampleRate;
            if (markerTimeInSeconds < itemLength) {
                SetTakeMarker(take, -1, "", &markerTimeInSeconds, nullptr);
            }
        }
    }

    // Offset marker colour
    int r = 255;
    int g = 0;
    int b = 0;

    int color = ColorToNative(r, g, b) | 0x1000000;

    auto offsetView = reader.samps(1);
    for (fluid::index i = 0; i < offsetView.size(); i++) {
        if (offsetView(i) > 0) {
            double markerTimeInSeconds =
                static_cast<double>(offsetView(i)) / sampleRate;
            if (markerTimeInSeconds < itemLength) {
                SetTakeMarker(take, -1, "", &markerTimeInSeconds, &color);
            }
        }
    }
    return true;
}

const char *AmpGateAlgorithm::GetName() const { return "Amp Gate"; }

int AmpGateAlgorithm::GetNumAlgorithmParams() const { return kNumParams; }

std::unique_ptr<IAlgorithm> AmpGateAlgorithm::CreateNew() const {
    return std::make_unique<AmpGateAlgorithm>(mApiProvider);
}
