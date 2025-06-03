#pragma once
#include "FlucomaAlgorithmBase.h"
#include "../../dependencies/flucoma-core/include/flucoma/clients/nrt/NMFClient.hpp"

class NMFAlgorithm : public AudioOutputAlgorithm<fluid::client::NRTThreadedNMFClient> {
public:
    enum Params {
        kComponents = 0,
        kIterations,
        kNumParams
    };

    NMFAlgorithm(ReacomaExtension* apiProvider);
    ~NMFAlgorithm() override;

    const char* GetName() const override;
    void RegisterParameters() override;
    int GetNumAlgorithmParams() const override;

protected:
    bool DoProcess(InputBufferT::type& sourceBuffer, int numChannels, int frameCount, int sampleRate) override;
    bool HandleResults(MediaItem* item, MediaItem_Take* take, int numChannels, int sampleRate) override;
};
