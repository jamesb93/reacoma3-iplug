#pragma once
#include "IAlgorithm.h"
#include <vector>
#include "../../dependencies/flucoma-core/include/flucoma/clients/nrt/NMFClient.hpp"
#include "../../dependencies/flucoma-core/include/flucoma/clients/common/FluidContext.hpp"

using namespace fluid::client;

class NMFAlgorithm : public IAlgorithm {
public:
    enum Params {
        kComponents = 0,
        kIterations,
//        kFFTSettings,
        kNumParams
    };

    NMFAlgorithm(ReacomaExtension* apiProvider);
    ~NMFAlgorithm() override;

    bool ProcessItem(MediaItem* item) override;
    const char* GetName() const override;
    void RegisterParameters() override;
    int GetNumAlgorithmParams() const override;
    void AddOutputToTake(MediaItem* item, BufferT::type output, int numChannels, int sampleRate, const std::string& suffix);

private:
    FluidContext mContext;
    NRTThreadedNMFClient::ParamSetType mParams;
    NRTThreadedNMFClient mClient;
};
