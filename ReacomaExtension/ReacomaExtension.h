#pragma once

#include "ReaperExt_include_in_plug_hdr.h"
#include "reaper_plugin.h"
#include <memory>
#include "Algorithms/NoveltySliceAlgorithm.h"
#include "Algorithms/HPSSAlgorithm.h"

class IAlgorithm;

using namespace iplug;
using namespace igraphics;

class ReacomaExtension : public ReaperExtBase
{
public:
    enum class Mode { Segment, Markers, Regions, Preview };
    
    enum EParams
    {
        kParamAlgorithmChoice = 0,
        kNumOwnParams
    };
    
    enum EAlgorithmChoice {
        kNoveltySlice = 0,
        kHPSS,
        kNumAlgorithmChoices
    };
        
    ReacomaExtension(reaper_plugin_info_t* pRec);
    void OnUIClose() override;
    void Process(Mode mode, bool force);
    


private:
    std::unique_ptr<NoveltySliceAlgorithm> mNoveltyAlgorithm;
    std::unique_ptr<HPSSAlgorithm> mHPSSAlgorithm;
    void OnParamChangeUI(int paramIdx, EParamSource source) override;
    void OnIdle() override;
    bool UpdateSelectedItems(bool force);
    void SetAlgorithmChoice(EAlgorithmChoice choice, bool triggerUIRelayout);
    void SetupUI(IGraphics* pGraphics);
    
    int mPrevTrackCount = 0;
    int mGUIToggle = 0;
    
    bool mNeedsPreview = false;
    
    double mLastUpdate = -1.0;
    
    int mNumSelectedItems = 0;
    
    std::vector<MediaItem*> mSelectedItems;
    bool mIsProcessingAsync = false;

    IAlgorithm* mCurrentActiveAlgorithmPtr = nullptr;
    EAlgorithmChoice mCurrentAlgorithmChoice = kNoveltySlice;
    bool mNeedsLayout = false;
};
