#pragma once
#include "../../dependencies/flucoma-core/include/flucoma/clients/rt/AmpGateClient.hpp"
#include "FlucomaAlgorithmBase.h"

class AmpGateAlgorithm
    : public FlucomaAlgorithm<fluid::client::NRTThreadedAmpGateClient> {
public:
    enum Params {
        kRampUpTime = 0,
        kRampDownTime,
        kOnThreshold,
        kOffThreshold,
        kMinEventDuration,
        kMinSilenceDuration,
        kMinTimeAboveThreshold,
        kMinTimeBelowThreshold,
        kUpwardLookupTime,
        kDownwardLookupTime,
        kHiPassFreq,
        kNumParams
    };

    AmpGateAlgorithm(IParameterProvider *apiProvider);
    ~AmpGateAlgorithm() override;

    const char *GetName() const override;
    std::vector<ParameterDescriptor> GetParamDescriptors() const override;
    int GetNumAlgorithmParams() const override;

    std::unique_ptr<IAlgorithm> CreateNew() const override;

protected:
    bool DoProcess(InputBufferT::type &sourceBuffer, int numChannels,
                   int frameCount, int sampleRate) override;
    bool HandleResults(MediaItem *item, MediaItem_Take *take, int numChannels,
                       int sampleRate) override;
};
