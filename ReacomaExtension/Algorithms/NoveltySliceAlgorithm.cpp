// NoveltySliceAlgorithm.cpp
#include "NoveltySliceAlgorithm.h"
#include "ReacomaExtension.h" // Include the full definition of ReacomaExtension
#include "ReaperExt_include_in_plug_hdr.h" // For MediaItem*, PCM_source*, etc.
// reaper_plugin_functions.h is likely not needed directly here if ReacomaExtension wraps calls

#include "../../dependencies/flucoma-core/include/flucoma/clients/rt/NoveltySliceClient.hpp"
#include "../../dependencies/flucoma-core/include/flucoma/clients/common/FluidContext.hpp"
#include "../VectorBufferAdaptor.h" // Make sure this path is correct

struct NoveltySliceAlgorithm::Impl {
    fluid::client::FluidContext mFluCoMaContext;
    fluid::client::NRTThreadingNoveltySliceClient::ParamSetType mFluCoMaParams;
    fluid::client::NRTThreadingNoveltySliceClient mFluCoMaClient;
    ReacomaExtension* mApiProvider; // Changed to ReacomaExtension*

    Impl(ReacomaExtension* apiProvider) : // Changed
        mFluCoMaContext{},
        mFluCoMaParams{fluid::client::NRTThreadingNoveltySliceClient::getParameterDescriptors(), fluid::FluidDefaultAllocator()},
        mFluCoMaClient{mFluCoMaParams, mFluCoMaContext},
        mApiProvider(apiProvider) // Changed
    {
        // Any further initialization
    }

    bool processItemImpl(MediaItem* item) {
        if (!item || !mApiProvider) return false; // Check if pointer is valid

        // Use the ReacomaExtension pointer to call API functions
        MediaItem_Take* take = mApiProvider->GetActiveTake(item);
        if (!take) return false;

        PCM_source* source = mApiProvider->GetMediaItemTake_Source(take);
        if (!source) return false;

        int sampleRate = mApiProvider->GetMediaSourceSampleRate(source);
        int numChannels = mApiProvider->GetMediaSourceNumChannels(source);
        double itemLength = mApiProvider->GetMediaItemInfo_Value(item, "D_LENGTH");
        double playrate = mApiProvider->GetMediaItemTakeInfo_Value(take, "D_PLAYRATE");

        // ... rest of your FluCoMa processing ...
        // mApiProvider->ShowConsoleMsg("Processing...\n"); // Example of calling another ReacomaExtension method

        return true;
    }
};

NoveltySliceAlgorithm::NoveltySliceAlgorithm(ReacomaExtension* apiProvider) // Changed
    : mImpl(std::make_unique<Impl>(apiProvider)) {} // Changed

NoveltySliceAlgorithm::~NoveltySliceAlgorithm() = default;

bool NoveltySliceAlgorithm::ProcessItem(MediaItem* item) {
    return mImpl->processItemImpl(item);
}

const char* NoveltySliceAlgorithm::GetName() const {
    return "NoveltySlice";
}
