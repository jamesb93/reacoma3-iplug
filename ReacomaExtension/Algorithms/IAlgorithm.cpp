//
//  IAlgorithm.cpp
//  ReacomaExtension
//
//  Created by James Bradbury on 25/5/2025.
//

// IAlgorithm.cpp
#include "IAlgorithm.h"

IAlgorithm::IAlgorithm(ReacomaExtension* apiProvider)
    : mApiProvider(apiProvider) {
    // Base class constructor. No specific Impl initialization here.
}

IAlgorithm::~IAlgorithm() = default; // Default implementation as it's virtual
