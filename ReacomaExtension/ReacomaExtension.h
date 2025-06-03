#pragma once

#include "ReaperExt_include_in_plug_hdr.h"
#include "reaper_plugin.h"
#include <memory>
#include <vector>
#include <deque> // NEW: For using a queue

#include "Algorithms/NoveltySliceAlgorithm.h"
#include "Algorithms/HPSSAlgorithm.h"
#include "Algorithms/NMFAlgorithm.h"

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
        kNMF,
        kNumAlgorithmChoices
    };
            
    ReacomaExtension(reaper_plugin_info_t* pRec);
    void OnUIClose() override;
    void Process(Mode mode, bool force);
    


private:
    std::unique_ptr<NoveltySliceAlgorithm> mNoveltyAlgorithm;
    std::unique_ptr<HPSSAlgorithm> mHPSSAlgorithm;
    std::unique_ptr<NMFAlgorithm> mNMFAlgorithm;
    
    void OnParamChangeUI(int paramIdx, EParamSource source) override;
    void OnIdle() override;
    bool UpdateSelectedItems(bool force);
    void SetAlgorithmChoice(EAlgorithmChoice choice, bool triggerUIRelayout);
    void SetupUI(IGraphics* pGraphics);

    void StartNextItemInQueue();
    
    int mPrevTrackCount = 0;
    int mGUIToggle = 0;
    
    bool mNeedsPreview = false;
    
    double mLastUpdate = -1.0;
    
    int mNumSelectedItems = 0;
    
    std::vector<MediaItem*> mSelectedItems;

    IAlgorithm* mCurrentActiveAlgorithmPtr = nullptr;
    EAlgorithmChoice mCurrentAlgorithmChoice = kNoveltySlice;
    
    // MODIFIED: State variables are now for managing a queue of items
    bool mIsProcessingBatch = false; // Renamed for clarity
    IAlgorithm* mBatchProcessingAlgorithm = nullptr; // Renamed for clarity
    MediaItem* mCurrentlyProcessingItem = nullptr; // Tracks the single item being processed now
    std::deque<MediaItem*> mProcessingQueue; // The queue of items waiting to be processed
    int mTotalQueueSize = 0;
    int mQueueProgress = 0;
    
    // WAS:
    // bool mIsProcessingAsync = false;
    // IAlgorithm* mAsyncProcessingAlgorithm = nullptr;
    // MediaItem* mAsyncProcessingItem = nullptr;
};
