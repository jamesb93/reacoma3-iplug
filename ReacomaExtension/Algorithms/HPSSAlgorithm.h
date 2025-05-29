#pragma once
#include "IAlgorithm.h"
#include <memory>
#include <vector>

class HPSSAlgorithm : public IAlgorithm {
public:
    enum Params {
        kHarmFilterSize = 0,
        kPercFilterSize,
        kNumParams
    };

    // enum EAlgorithmOptions {
    //     kSpectrum = 0,
    //     kMFCC,
    //     kChroma,
    //     kPitch,
    //     kLoudness,
    //     kNumAlgorithmOptions
    // };

    HPSSAlgorithm(ReacomaExtension* apiProvider);
    ~HPSSAlgorithm() override;

    bool ProcessItem(MediaItem* item) override;
    const char* GetName() const override;
    void RegisterParameters() override;
    int GetNumAlgorithmParams() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
