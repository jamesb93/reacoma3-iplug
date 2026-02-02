#pragma once
#include "../../dependencies/flucoma-core/include/flucoma/clients/rt/HPSSClient.hpp"
#include "FlucomaAlgorithmBase.h"

class HPSSAlgorithm
    : public AudioOutputAlgorithm<fluid::client::NRTThreadedHPSSClient> {
public:
    enum Params {
        kHarmFilterSize = 0,
        kPercFilterSize,
        kWindowSize,
        kHopSize,
        kFFTSize,
        kNumParams
    };

    HPSSAlgorithm(IParameterProvider *apiProvider);
    ~HPSSAlgorithm() override;

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
