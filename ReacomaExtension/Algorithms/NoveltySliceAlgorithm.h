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
        kFilterSize,
        kAlgorithm,
        kNumParams // Keep this as the last element to count parameters
    };

    // Enum for algorithm options (if specific to this algorithm's parameters)
    enum EAlgorithmOptions {
        kSpectrum = 0,
        kMFCC,
        kChroma,
        kPitch,
        kLoudness,
        kNumAlgorithmOptions // Keep this as the last element
    };

    // Constructor now takes a ReacomaExtension pointer and passes it to the base
    NoveltySliceAlgorithm(ReacomaExtension* apiProvider);
    // Mark destructor as override
    ~NoveltySliceAlgorithm() override;

    // Implement pure virtual functions from IAlgorithm
    bool ProcessItem(MediaItem* item) override;
    const char* GetName() const override;
    void RegisterParameters() override;

private:
    // Pimpl idiom specific to this concrete implementation
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
