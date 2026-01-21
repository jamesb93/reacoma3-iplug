#include "TransientSliceAlgorithm.h"
#include "IPlugParameter.h"
#include "ReacomaExtension.h"

TransientSliceAlgorithm::TransientSliceAlgorithm(ReacomaExtension *apiProvider)
    : FlucomaAlgorithm<NRTThreadedTransientSliceClient>(apiProvider) {}

TransientSliceAlgorithm::~TransientSliceAlgorithm() = default;

std::vector<ParameterDescriptor>
TransientSliceAlgorithm::GetParamDescriptors() const {
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
    descriptors.push_back({ParameterDescriptor::Int, "Minimum Slice Length",
                           1000.0, 0.0, 10000.0});
    return descriptors;
}

bool TransientSliceAlgorithm::DoProcess(InputBufferT::type &sourceBuffer,
                                        int numChannels, int frameCount,
                                        int sampleRate) {
    int estimatedSlices = std::max(1, static_cast<int>(frameCount / 1024.0));
    auto outBuffer =
        std::make_shared<MemoryBufferAdaptor>(1, estimatedSlices, sampleRate);
    auto slicesOutputBuffer = fluid::client::BufferT::type(outBuffer);

    auto order = GetParamValue(kOrder);
    auto blockSize = GetParamValue(kBlockSize);
    auto padding = GetParamValue(kPadding);
    auto skew = GetParamValue(kSkew);
    auto fwd = GetParamValue(kThreshFwd);
    auto bwd = GetParamValue(kThreshBack);
    auto winSize = GetParamValue(kWinSize);
    auto clumpLength = GetParamValue(kClump);
    auto minSliceLength = GetParamValue(kMinSliceLength);

    // FluCoMa Client Parameter Indices
    constexpr int kFlucomaInputAudio = 0;
    constexpr int kFlucomaStartChan = 1;
    constexpr int kFlucomaNumChans = 2;
    constexpr int kFlucomaStartFrame = 3;
    constexpr int kFlucomaNumFrames = 4;
    constexpr int kFlucomaOnsets = 5;
    constexpr int kFlucomaOrder = 6;
    constexpr int kFlucomaBlockSize = 7;
    constexpr int kFlucomaPadding = 8;
    constexpr int kFlucomaSkew = 9;
    constexpr int kFlucomaThreshFwd = 10;
    constexpr int kFlucomaThreshBack = 11;
    constexpr int kFlucomaWinSize = 12;
    constexpr int kFlucomaClump = 13;
    constexpr int kFlucomaMinSliceLength = 14;

    mParams.template set<kFlucomaInputAudio>(std::move(sourceBuffer), nullptr);
    mParams.template set<kFlucomaStartChan>(std::move(LongT::type(0)), nullptr);
    mParams.template set<kFlucomaNumChans>(std::move(LongT::type(-1)), nullptr);
    mParams.template set<kFlucomaStartFrame>(std::move(LongT::type(0)),
                                             nullptr);
    mParams.template set<kFlucomaNumFrames>(std::move(LongT::type(-1)),
                                            nullptr);
    mParams.template set<kFlucomaOnsets>(std::move(slicesOutputBuffer),
                                         nullptr);
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
    mParams.template set<kFlucomaMinSliceLength>(
        std::move(LongT::type(minSliceLength)), nullptr);

    mClient = NRTThreadedTransientSliceClient(mParams, mContext);
    mClient.setSynchronous(false);
    mClient.enqueue(mParams);
    Result result = mClient.process();
    return result.ok();
}

bool TransientSliceAlgorithm::HandleResults(MediaItem *item,
                                            MediaItem_Take *take,
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

const char *TransientSliceAlgorithm::GetName() const {
    return "Transient Slice";
}

int TransientSliceAlgorithm::GetNumAlgorithmParams() const {
    return kNumParams;
}

std::unique_ptr<IAlgorithm> TransientSliceAlgorithm::CreateNew() const {
    return std::make_unique<TransientSliceAlgorithm>(mApiProvider);
}
