#pragma once
#include "IAlgorithm.h"
#include <vector>
#include "../../dependencies/flucoma-core/include/flucoma/clients/rt/NoveltySliceClient.hpp"
#include "../../dependencies/flucoma-core/include/flucoma/clients/common/FluidContext.hpp"

using namespace fluid::client;

class NoveltySliceAlgorithm : public IAlgorithm {
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

    bool ProcessItem(MediaItem* item) override;
    const char* GetName() const override;
    void RegisterParameters() override;
    int GetNumAlgorithmParams() const override;

private:
    FluidContext mContext;
    NRTThreadingNoveltySliceClient::ParamSetType mParams;
    NRTThreadingNoveltySliceClient mClient;
};
