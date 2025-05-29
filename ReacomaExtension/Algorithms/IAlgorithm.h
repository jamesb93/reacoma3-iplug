#pragma once
#include <memory>
#include <vector>

class MediaItem;
class ReacomaExtension;

class IAlgorithm {
public:
    // Constructor now takes a ReacomaExtension pointer
    IAlgorithm(ReacomaExtension* apiProvider);
    // CRUCIAL: Destructor MUST be virtual for polymorphic base classes
    virtual ~IAlgorithm();

    // Pure virtual function: Concrete derived classes MUST implement this.
    virtual bool ProcessItem(MediaItem* item) = 0;

    // Pure virtual function: Each concrete algorithm should provide its name.
    virtual const char* GetName() const = 0;

    // Pure virtual function: Each concrete algorithm will have its own parameters to register.
    // This method will set mBaseParamIdx.
    virtual void RegisterParameters() = 0;

    // Helper to get the global parameter index for an algorithm-specific parameter enum
    int GetGlobalParamIdx(int algorithmParamEnum) const {
        return mBaseParamIdx + algorithmParamEnum;
    }

    // Gets the number of parameters this algorithm registers
    virtual int GetNumAlgorithmParams() const = 0;

protected:
    // Protected member to be accessible by derived classes
    ReacomaExtension* mApiProvider;
    int mBaseParamIdx = 0; // Starting index of parameters registered by this algorithm
};
