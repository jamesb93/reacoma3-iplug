#pragma once
#include "../../dependencies/flucoma-core/include/flucoma/clients/nrt/NMFClient.hpp"
#include "FlucomaAlgorithmBase.h"

class NMFAlgorithm
    : public AudioOutputAlgorithm<fluid::client::NRTThreadedNMFClient> {
public:
    enum Params {
        kComponents = 0,
        kIterations,
        kWindowSize,
        kHopSize,
        kFFTSize,
        kNumParams
    };

    NMFAlgorithm(ReacomaExtension *apiProvider);
    ~NMFAlgorithm() override;

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
