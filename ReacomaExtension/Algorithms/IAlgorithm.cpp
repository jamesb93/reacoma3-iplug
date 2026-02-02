#include "IAlgorithm.h"
#include "IParameterProvider.h"

IAlgorithm::IAlgorithm(IParameterProvider *apiProvider)
    : mApiProvider(apiProvider) {}

IAlgorithm::~IAlgorithm() = default;

void IAlgorithm::SyncParameters() {
    int numParams = GetNumAlgorithmParams();
    if (mParamValues.size() < numParams)
        InitParamValues(numParams);

    if (mApiProvider) {
        for (int i = 0; i < numParams; ++i) {
            SetParamValue(
                i, mApiProvider->GetParameterValue(GetGlobalParamIdx(i)));
        }
    }
}
