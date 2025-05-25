// NoveltySliceAlgorithm.cpp
#include "NoveltySliceAlgorithm.h"

#include "../../dependencies/flucoma-core/include/flucoma/clients/rt/NoveltySliceClient.hpp"
#include "../../dependencies/flucoma-core/include/flucoma/clients/common/FluidContext.hpp"
#include "../VectorBufferAdaptor.h"

struct NoveltySliceAlgorithm::Impl {
    fluid::client::FluidContext mFluCoMaContext;
    fluid::client::NRTThreadingNoveltySliceClient::ParamSetType mFluCoMaParams;
    fluid::client::NRTThreadingNoveltySliceClient mFluCoMaClient;

    Impl() : mFluCoMaContext{},
             mFluCoMaParams{fluid::client::NRTThreadingNoveltySliceClient::getParameterDescriptors(), fluid::FluidDefaultAllocator()},
             mFluCoMaClient{mFluCoMaParams, mFluCoMaContext}
    {
        // Any further initialization
    }

    bool processItemImpl(MediaItem* item) {
        if (!item) return false;
        std::vector<double> output;
        MediaItem_Take* take = GetActiveTake(item);
        PCM_source* source = GetMediaItemTake_Source(take);
        int sampleRate = GetMediaSourceSampleRate(source);
        int numChannels = GetMediaSourceNumChannels(source);
        double itemLength = GetMediaItemInfo_Value(item, "D_LENGTH");
        double playrate = GetMediaItemTakeInfo_Value(take, "D_PLAYRATE");
        return true;
    }
};

NoveltySliceAlgorithm::NoveltySliceAlgorithm() : mImpl(std::make_unique<Impl>()) {}
NoveltySliceAlgorithm::~NoveltySliceAlgorithm() = default; // Or {}

bool NoveltySliceAlgorithm::ProcessItem(MediaItem* item) {
    return mImpl->processItemImpl(item);
}

const char* NoveltySliceAlgorithm::GetName() const {
    return "NoveltySlice";
}
