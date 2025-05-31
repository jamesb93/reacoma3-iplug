#pragma once
#include "IAlgorithm.h"
#include <vector>
#include "../../dependencies/flucoma-core/include/flucoma/clients/rt/HPSSClient.hpp"
#include "../../dependencies/flucoma-core/include/flucoma/clients/common/FluidContext.hpp"

using namespace fluid::client;

class HPSSAlgorithm : public IAlgorithm {
public:
    enum Params {
        kHarmFilterSize = 0,
        kPercFilterSize,
        kNumParams
    };

    HPSSAlgorithm(ReacomaExtension* apiProvider);
    ~HPSSAlgorithm() override;

    bool ProcessItem(MediaItem* item) override;
    const char* GetName() const override;
    void RegisterParameters() override;
    int GetNumAlgorithmParams() const override;

private:
    FluidContext mContext;
    NRTThreadedHPSSClient::ParamSetType mParams;
    NRTThreadedHPSSClient mClient;
};