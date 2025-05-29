// NoveltySliceAlgorithm.h
#pragma once
#include "IAlgorithm.h" // Include the new abstract base class
#include <memory>       // For std::unique_ptr
#include <vector>

class NoveltySliceAlgorithm : public IAlgorithm { // Inherit from IAlgorithm
public:
    // Enum for algorithm-specific parameters
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
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
