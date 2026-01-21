#pragma once
#include "flucoma/clients/rt/AmpSliceClient.hpp"
#include "SlicingAlgorithm.h"

class AmpSliceAlgorithm
    : public SlicingAlgorithm<fluid::client::NRTThreadedAmpSliceClient> {
public:
    enum Params {
        kFastRampUpTime = 0,
        kFastRampDownTime,
        kSlowRampUpTime,
        kSlowRampDownTime,
        kOnThreshold,
        kOffThreshold,
        kSilenceThreshold,
        kDebounce,
        kHiPassFreq,
        kNumParams
    };

    AmpSliceAlgorithm(ReacomaExtension *apiProvider);
    ~AmpSliceAlgorithm() override;

    const char *GetName() const override;
    std::vector<ParameterDescriptor> GetParamDescriptors() const override;
    int GetNumAlgorithmParams() const override;

    std::unique_ptr<IAlgorithm> CreateNew() const override;

protected:
    BufferT::type &GetSlicesBuffer() override;
    bool DoProcess(InputBufferT::type &sourceBuffer, int numChannels,
                   int frameCount, int sampleRate) override;
};
