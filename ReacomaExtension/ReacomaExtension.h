#pragma once

#include "ReaperExt_include_in_plug_hdr.h"
#include "reaper_plugin.h"

#include <deque>
#include <list>
#include <memory>
#include <thread>
#include <vector>

#include "ibmplexmono.hpp"
#include "roboto.hpp"

#include "IAlgorithm.h"
#include "IControls.h"
#include "IPlugParameter.h"

#include "Components/ReacomaButton.h"
#include "Components/ReacomaParamTextControl.h"
#include "Components/ReacomaProgressBar.h"
#include "Components/ReacomaSegmented.h"
#include "Components/ReacomaSlider.h"

#include "Algorithms/HPSSAlgorithm.h"
#include "Algorithms/NMFAlgorithm.h"
#include "Algorithms/TransientAlgorithm.h"
#include "Algorithms/TransientSliceAlgorithm.h"
#include "Algorithms/NoveltySliceAlgorithm.h"
#include "Algorithms/OnsetSliceAlgorithm.h"

class IAlgorithm;
class ProcessingJob;
class ReacomaProgressBar;

using namespace iplug;
using namespace igraphics;

class ReacomaExtension : public ReaperExtBase {
  public:
    enum class Mode { Segment, Regions, ProcessAudio };

    enum EParams { kParamAlgorithmChoice = 0, kNumOwnParams };

    enum EAlgorithmChoice {
        kNoveltySlice = 0,
        kOnsetSlice,
        kTransientSlice,
        kHPSS,
        kNMF,
        kTransients,
        kNumAlgorithmChoices
    };

    ReacomaExtension(reaper_plugin_info_t *pRec);
    void OnUIClose() override;
    void Process(Mode mode, bool force);
    void CancelRunningJobs();
    void ResetUIState();

    NoveltySliceAlgorithm *GetNoveltySliceAlgorithm() const {
        return mNoveltyAlgorithm.get();
    }
    HPSSAlgorithm *GetHPSSAlgorithm() const { return mHPSSAlgorithm.get(); }
    NMFAlgorithm *GetNMFAlgorithm() const { return mNMFAlgorithm.get(); }
    TransientAlgorithm *GetTransientsAlgorithm() const {
        return mTransientsAlgorithm.get();
    }
    TransientSliceAlgorithm *GetTransientSliceAlgorithm() const {
        return mTransientSliceAlgorithm.get();
    }
    OnsetSliceAlgorithm *GetOnsetSliceAlgorithm() const {
        return mOnsetSliceAlgorithm.get();
    }

  private:
    std::unique_ptr<NoveltySliceAlgorithm> mNoveltyAlgorithm;
    std::unique_ptr<HPSSAlgorithm> mHPSSAlgorithm;
    std::unique_ptr<NMFAlgorithm> mNMFAlgorithm;
    std::unique_ptr<TransientAlgorithm> mTransientsAlgorithm;
    std::unique_ptr<OnsetSliceAlgorithm> mOnsetSliceAlgorithm;
    std::unique_ptr<TransientSliceAlgorithm> mTransientSliceAlgorithm;

    void OnParamChangeUI(int paramIdx, EParamSource source) override;
    void OnIdle() override;
    void SetAlgorithmChoice(EAlgorithmChoice choice, bool triggerUIRelayout);
    void SetupUI(IGraphics *pGraphics);
    void StartNextItemInQueue();

    int mGUIToggle = 0;

    IAlgorithm *mCurrentActiveAlgorithmPtr = nullptr;
    EAlgorithmChoice mCurrentAlgorithmChoice = kNoveltySlice;
    Mode mCurrentProcessingMode;

    unsigned int mConcurrencyLimit = 1;
    std::deque<MediaItem *> mPendingItemsQueue;
    std::list<std::unique_ptr<ProcessingJob>> mActiveJobs;
    std::deque<std::unique_ptr<ProcessingJob>> mFinalizationQueue;
    std::deque<MediaItem *> mProcessingQueue;

    ReacomaProgressBar *mProgressBar = nullptr;
    ReacomaButton *mCancelButton = nullptr;
    size_t mTotalBatchItems = 0;
    double mLastReportedProgress = 0.0;

    ReaProject *mBatchUndoProject = nullptr;
    bool mIsProcessingBatch = false;
    bool mIsCancellationRequested = false;
};
