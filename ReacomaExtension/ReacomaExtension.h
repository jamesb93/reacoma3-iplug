// ReacomaExtension.h
#pragma once

#include "ReaperExt_include_in_plug_hdr.h"
#include "reaper_plugin.h"
#include <memory> // For std::unique_ptr
#include "Algorithms/NoveltySliceAlgorithm.h"
#include "Algorithms/HPSSAlgorithm.h"

// Forward declare IAlgorithm
class IAlgorithm;

using namespace iplug;
using namespace igraphics;

class ReacomaExtension : public ReaperExtBase
{
public:
    enum class Mode { Segment, Markers, Regions, Preview };
    
    enum EParams
    {
        kParamAlgorithmChoice = 0,  // This declares kParamAlgorithmChoice as an integer (0)
        // If ReacomaExtension had other top-level parameters, they would go here.
        kNumOwnParams               // A counter for ReacomaExtension's own parameters.
                                    // This will be useful if you add more later, and for
                                    // algorithm parameter registration to start after these.
    };
    
    enum EAlgorithmChoice {
        kNoveltySlice = 0,
        kHPSS,
        kNumAlgorithmChoices // Keep this last for counting
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
    
    int mPrevTrackCount = 0;
    int mGUIToggle = 0;
    
    bool mNeedsPreview = false;
    
    double mLastUpdate = -1.0;
    
    int mNumSelectedItems = 0;
    
    std::vector<MediaItem*> mSelectedItems;
    bool mIsProcessingAsync = false;

    IAlgorithm* mCurrentActiveAlgorithmPtr = nullptr; // Points to the active algorithm instance
    EAlgorithmChoice mCurrentAlgorithmChoice = kNoveltySlice;
    bool mForceParameterRedraw = true;
};
