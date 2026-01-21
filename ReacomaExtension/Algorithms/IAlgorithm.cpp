#include "IAlgorithm.h"
#include "ReacomaExtension.h"

IAlgorithm::IAlgorithm(ReacomaExtension *apiProvider)
    : mApiProvider(apiProvider) {}

IAlgorithm::~IAlgorithm() = default;

void IAlgorithm::SyncParameters() {
    int numParams = GetNumAlgorithmParams();
    if (mParamValues.size() < numParams)
        InitParamValues(numParams);

    for (int i = 0; i < numParams; ++i) {
        SetParamValue(i, mApiProvider->GetParam(GetGlobalParamIdx(i))->Value());
    }
}
