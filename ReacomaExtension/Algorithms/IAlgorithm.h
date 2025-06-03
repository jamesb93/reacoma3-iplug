#pragma once
#include <memory>
#include <vector>

class MediaItem;
class ReacomaExtension;

class IAlgorithm {
public:
    IAlgorithm(ReacomaExtension* apiProvider);
    virtual ~IAlgorithm();

    // MODIFIED: The old ProcessItem is now split into three phases for async operations
    virtual bool StartProcessItemAsync(MediaItem* item) = 0;
    virtual bool IsFinished() = 0;
    virtual bool FinalizeProcess(MediaItem* item) = 0;
    // WAS: virtual bool ProcessItem(MediaItem* item) = 0;


    // Pure virtual function: Each concrete algorithm should provide its name.
    virtual const char* GetName() const = 0;

    // Pure virtual function: Each concrete algorithm will have its own parameters to register.
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
