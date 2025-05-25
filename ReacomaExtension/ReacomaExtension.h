#pragma once

#include "ReaperExt_include_in_plug_hdr.h"
#include "reaper_plugin.h"
#include "../dependencies/flucoma-core/include/flucoma/clients/common/FluidContext.hpp"


using namespace iplug;
using namespace igraphics;


class ReacomaExtension : public ReaperExtBase
{
public:
    enum class Mode { Segment, Markers, Regions, Preview };
        
    ReacomaExtension(reaper_plugin_info_t* pRec);
    void OnUIClose() override;
    
    void Process(Mode mode, bool force);

private:
    
    void OnParamChangeUI(int paramIdx, EParamSource source) override;
    void OnIdle() override;
    
    bool UpdateSelectedItems(bool force);
    
    int mPrevTrackCount = 0;
    int mGUIToggle = 0;
    
    bool mNeedsPreview = false;
    
    double mLastUpdate = -1.0;
    
    int mNumSelectedItems = 0;
    
    std::vector<MediaItem*> mSelectedItems;
    bool mIsProcessingAsync = false;
};

