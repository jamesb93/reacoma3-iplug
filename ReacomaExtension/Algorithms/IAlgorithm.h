#pragma once
#include <memory>
#include <string>
#include <vector>

class MediaItem;
class ReacomaExtension;

struct ParameterDescriptor {
    enum Type { Double, Int, Enum };
    Type type;
    std::string name;
    double defaultVal;
    double minVal;
    double maxVal;
    double step = 0.0;
    std::vector<std::string> enumLabels;
};

class IAlgorithm {
public:
    IAlgorithm(ReacomaExtension *apiProvider);
    virtual ~IAlgorithm();

    virtual bool StartProcessItemAsync(MediaItem *item) = 0;
    virtual bool IsFinished() = 0;
    virtual bool FinalizeProcess(MediaItem *item) = 0;

    virtual double GetProgress() = 0;
    virtual void Cancel() = 0;

    virtual const char *GetName() const = 0;
    virtual std::vector<ParameterDescriptor> GetParamDescriptors() const = 0;

    void SetParamValue(int index, double value) {
        if (index >= 0 && index < mParamValues.size()) {
            mParamValues[index] = value;
        }
    }

    double GetParamValue(int index) const {
        return (index >= 0 && index < mParamValues.size()) ? mParamValues[index]
                                                           : 0.0;
    }

    void InitParamValues(size_t count) { mParamValues.assign(count, 0.0); }

    void SyncParameters();

    int GetGlobalParamIdx(int algorithmParamEnum) const {
        return mBaseParamIdx + algorithmParamEnum;
    }

    virtual int GetNumAlgorithmParams() const = 0;
    int GetBaseParamIdx() const { return mBaseParamIdx; }
    void SetBaseParamIdx(int idx) { mBaseParamIdx = idx; }

    virtual bool SupportsSegmentation() = 0;
    virtual bool SupportsRegions() = 0;
    virtual bool CreatesTakes() = 0;

    virtual std::unique_ptr<IAlgorithm> CreateNew() const = 0;

protected:
    ReacomaExtension *mApiProvider;
    int mBaseParamIdx = 0;
    std::vector<double> mParamValues;
};
