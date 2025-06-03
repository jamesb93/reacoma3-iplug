#pragma once
#include "../../dependencies/flucoma-core/include/flucoma/clients/rt/NoveltySliceClient.hpp"
#include "FlucomaAlgorithmBase.h"

class NoveltySliceAlgorithm : public FlucomaAlgorithm<fluid::client::NRTThreadingNoveltySliceClient> {
public:
    enum Params {
        kThreshold = 0,
        kKernelSize,
        kFilterSize,
        kMinSliceLength,
        kAlgorithm,
        kNumParams
    };
    
    enum EAlgorithmOptions {
        kSpectrum = 0,
        kMFCC,
        kChroma,
        kPitch,
        kLoudness,
        kNumAlgorithmOptions
    };

    NoveltySliceAlgorithm(ReacomaExtension* apiProvider);
    ~NoveltySliceAlgorithm() override;

    const char* GetName() const override;
    void RegisterParameters() override;
    int GetNumAlgorithmParams() const override;

protected:
    bool DoProcess(InputBufferT::type& sourceBuffer, int numChannels, int frameCount, int sampleRate) override;
    bool HandleResults(MediaItem* item, MediaItem_Take* take, int numChannels, int sampleRate) override;
};
