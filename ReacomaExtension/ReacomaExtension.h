#pragma once

#include "ReaperExt_include_in_plug_hdr.h"
#include "reaper_plugin.h"
#include <memory>
#include <vector>
#include <deque>
#include <thread>
#include <list>

#include "Algorithms/NoveltySliceAlgorithm.h"
#include "Algorithms/HPSSAlgorithm.h"
#include "Algorithms/NMFAlgorithm.h"

class IAlgorithm;
class ProcessingJob;

using namespace iplug;
using namespace igraphics;

class ReacomaExtension : public ReaperExtBase
{
public:
    enum class Mode { Segment, Markers, Regions, Preview };
    
    enum EParams {
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
    void SetAlgorithmChoice(EAlgorithmChoice choice, bool triggerUIRelayout);
    void SetupUI(IGraphics* pGraphics);
    void StartNextItemInQueue();
    
    IAlgorithm* mCurrentActiveAlgorithmPtr = nullptr;
    EAlgorithmChoice mCurrentAlgorithmChoice = kNoveltySlice;
    
    bool mIsProcessingBatch = false;
    unsigned int mConcurrencyLimit = 1;

    std::deque<MediaItem*> mPendingItemsQueue;

    std::list<std::unique_ptr<ProcessingJob>> mActiveJobs;

    std::deque<std::unique_ptr<ProcessingJob>> mFinalizationQueue;
    
    ReaProject* mBatchUndoProject = nullptr;
    
    int mGUIToggle = 0;
};
