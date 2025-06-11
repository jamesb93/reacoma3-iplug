#define REAPERAPI_IMPLEMENT

#include "ReacomaExtension.h"
#include "ReaperExt_include_in_plug_src.h"

#include <algorithm>
#include <deque>

#include "Algorithms/ProcessingJob.h"
#include "Components/ReacomaButton.h"
#include "Components/ReacomaParamTextControl.h"
#include "Components/ReacomaProgressBar.h"
#include "Components/ReacomaSegmented.h"

#include "IControls.h"

template <ReacomaExtension::Mode M> struct ProcessAction {
    void operator()(IControl *pCaller) {
        static_cast<ReacomaExtension *>(pCaller->GetDelegate())
            ->Process(M, true);
    }
};

ReacomaExtension::ReacomaExtension(reaper_plugin_info_t *pRec)
    : ReaperExtBase(pRec) {
    IMPAPI(CountSelectedMediaItems);
    IMPAPI(GetSelectedMediaItem);
    IMPAPI(GetItemProjectContext);
    IMPAPI(GetActiveTake);
    IMPAPI(GetMediaItemTake_Source);
    IMPAPI(GetMediaSourceSampleRate);
    IMPAPI(GetMediaSourceNumChannels);
    IMPAPI(GetMediaItemTakeInfo_Value);
    IMPAPI(GetMediaItemInfo_Value);
    IMPAPI(GetMediaSourceLength);
    IMPAPI(GetNumTakeMarkers);
    IMPAPI(IsMediaItemSelected);
    IMPAPI(DeleteTakeMarker);
    IMPAPI(ColorToNative);
    IMPAPI(SplitMediaItem);
    IMPAPI(DeleteTrackMediaItem);
    IMPAPI(SetTakeMarker);
    IMPAPI(AddProjectMarker2);
    IMPAPI(GetMediaItem_Track);
    IMPAPI(Undo_BeginBlock2);
    IMPAPI(Undo_EndBlock2);
    IMPAPI(UpdateArrange);
    IMPAPI(UpdateTimeline);
    IMPAPI(PCM_Source_CreateFromSimple);
    IMPAPI(AddTakeToMediaItem);
    IMPAPI(GetSetMediaItemTakeInfo);
    IMPAPI(PCM_Source_BuildPeaks);
    IMPAPI(GetProjectPath);
    IMPAPI(PCM_Sink_Create);
    IMPAPI(PCM_Sink_CreateEx);
    IMPAPI(PCM_Source_CreateFromFile);
    IMPAPI(GetMediaSourceParent);
    IMPAPI(GetMediaSourceFileName);
    IMPAPI(GetProjectPathEx);
    IMPAPI(GetSetProjectInfo_String);
    IMPAPI(SetMediaItemInfo_Value);

    mMakeGraphicsFunc = [&]() {
        return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS);
    };

    RegisterAction(
        "Reacoma: Show/Hide UI",
        [&]() {
            ShowHideMainWindow();
            mGUIToggle = !mGUIToggle;
        },
        true, &mGUIToggle);

    AddParam();
    GetParam(kParamAlgorithmChoice)
        ->InitEnum("Algorithm", kNoveltySlice, kNumAlgorithmChoices);
    GetParam(kParamAlgorithmChoice)
        ->SetDisplayText(kNoveltySlice, "Novelty Slice");
    GetParam(kParamAlgorithmChoice)->SetDisplayText(kOnsetSlice, "Onset Slice");
    GetParam(kParamAlgorithmChoice)
        ->SetDisplayText(kTransientSlice, "Transients");
    GetParam(kParamAlgorithmChoice)->SetDisplayText(kHPSS, "HPSS");
    GetParam(kParamAlgorithmChoice)->SetDisplayText(kNMF, "NMF");
    GetParam(kParamAlgorithmChoice)->SetDisplayText(kTransients, "Transients");
    GetParam(kParamAlgorithmChoice)->SetDisplayText(kAmpGate, "Amp Gate");

    mNoveltyAlgorithm = std::make_unique<NoveltySliceAlgorithm>(this);
    mNoveltyAlgorithm->RegisterParameters();

    mHPSSAlgorithm = std::make_unique<HPSSAlgorithm>(this);
    mHPSSAlgorithm->RegisterParameters();

    mNMFAlgorithm = std::make_unique<NMFAlgorithm>(this);
    mNMFAlgorithm->RegisterParameters();

    mOnsetSliceAlgorithm = std::make_unique<OnsetSliceAlgorithm>(this);
    mOnsetSliceAlgorithm->RegisterParameters();

    mTransientsAlgorithm = std::make_unique<TransientAlgorithm>(this);
    mTransientsAlgorithm->RegisterParameters();

    mTransientSliceAlgorithm = std::make_unique<TransientSliceAlgorithm>(this);
    mTransientSliceAlgorithm->RegisterParameters();

    mAmpGateAlgorithm = std::make_unique<AmpGateAlgorithm>(this);
    mAmpGateAlgorithm->RegisterParameters();

    SetAlgorithmChoice(kNoveltySlice, false);

    mLayoutFunc = [&](IGraphics *pGraphics) { SetupUI(pGraphics); };
}

void ReacomaExtension::OnUIClose() { mGUIToggle = 0; }

void ReacomaExtension::SetupUI(IGraphics *pGraphics) {
    const IRECT bounds = pGraphics->GetBounds();
    const IColor swissBackgroundColor = COLOR_WHITE;

    if (pGraphics->NControls()) {
        IControl *pBG = pGraphics->GetBackgroundControl();
        if (pBG) {
            pBG->SetTargetAndDrawRECTs(bounds);
        }
    }

    pGraphics->RemoveAllControls();
    mProgressBar = nullptr;
    mCancelButton = nullptr;

    pGraphics->EnableMouseOver(true);
    pGraphics->LoadFont("ibmplex", (void *)IBMPLEXMONO, IBMPLEXMONO_length);
    pGraphics->LoadFont("Roboto-Regular", (void *)ROBOTO_REGULAR,
                        ROBOTO_REGULAR_length);
    pGraphics->AttachPanelBackground(swissBackgroundColor);

    float globalFramePadding = 20.f;
    float topContentMargin = 10.f;
    float bottomContentMargin = 10.f;
    float actionButtonHeight = 30.f;
    float buttonPadding = 1.f;
    float controlVisualHeight = 25.f;
    float verticalSpacing = 10.f;

    IText segmentedTextStyle(14.f, COLOR_BLACK, "ibmplex");
    IColor activeColor = DEFAULT_PRCOLOR;
    IColor inactiveColor = DEFAULT_BGCOLOR;

    IRECT mainContentArea = bounds.GetPadded(-globalFramePadding);
    mainContentArea.T += topContentMargin;
    mainContentArea.B -= bottomContentMargin;

    IRECT remainingArea = mainContentArea;

    IRECT bottomUtilityRowBounds =
        remainingArea.GetFromBottom(controlVisualHeight);
    remainingArea.B -= (bottomUtilityRowBounds.H() + verticalSpacing);

    IRECT actionButtonRowBounds =
        remainingArea.GetFromBottom(actionButtonHeight);
    remainingArea.B -= (actionButtonHeight + verticalSpacing);

    IRECT currentLayoutBounds = remainingArea;

    IRECT algorithmSelectorRect;
    if (currentLayoutBounds.H() >= controlVisualHeight) {
        algorithmSelectorRect =
            currentLayoutBounds.GetFromTop(controlVisualHeight);
        currentLayoutBounds.T = algorithmSelectorRect.B + verticalSpacing;
        std::vector<std::string> algoLabels;
        IParam *pAlgoChoiceParam = GetParam(kParamAlgorithmChoice);
        if (pAlgoChoiceParam) {
            for (int i = 0; i < pAlgoChoiceParam->GetMax() + 1; ++i) {
                algoLabels.push_back(pAlgoChoiceParam->GetDisplayTextAtIdx(i));
            }
        }
        pGraphics->AttachControl(new ReacomaSegmented(
            algorithmSelectorRect, kParamAlgorithmChoice, algoLabels,
            segmentedTextStyle, activeColor, inactiveColor));
    }

    if (!mCurrentActiveAlgorithmPtr) {
        return;
    }

    int numAlgoParams = mCurrentActiveAlgorithmPtr->GetNumAlgorithmParams();
    for (int i = 0; i < numAlgoParams; ++i) {
        if (currentLayoutBounds.H() < controlVisualHeight)
            break;

        int globalParamIdx = mCurrentActiveAlgorithmPtr->GetGlobalParamIdx(i);
        IParam *pParam = GetParam(globalParamIdx);
        IRECT controlCellRect =
            currentLayoutBounds.GetFromTop(controlVisualHeight);

        IParam::EParamType type = pParam->Type();

        if (type == IParam::EParamType::kTypeDouble ||
            (type == IParam::EParamType::kTypeInt)) {
            IRECT sliderBounds = controlCellRect.GetVPadded(-5.f);
            const IText ibmPlexTextStyle(14.f, COLOR_BLACK, "ibmplex");
            auto *textControl = new ReacomaParamTextControl(
                sliderBounds, globalParamIdx, ibmPlexTextStyle);
            pGraphics->AttachControl(textControl);
        } else if (type == IParam::EParamType::kTypeEnum &&
                   pParam->GetMax() > 0) {
            std::vector<std::string> labels;
            for (int val = 0; val <= pParam->GetMax(); ++val) {
                const char *displayText = pParam->GetDisplayTextAtIdx(val);
                labels.push_back(displayText);
            }

            if (!labels.empty()) {
                pGraphics->AttachControl(new ReacomaSegmented(
                    controlCellRect, globalParamIdx, labels, segmentedTextStyle,
                    activeColor, inactiveColor));
            }
        }

        currentLayoutBounds.T = controlCellRect.B + verticalSpacing;
    }

    struct ButtonInfo {
        IActionFunction function;
        const char *label;
    };
    std::vector<ButtonInfo> buttonsToCreate;

    if (mCurrentActiveAlgorithmPtr->SupportsSegmentation()) {
        buttonsToCreate.push_back({ProcessAction<Mode::Segment>{}, "Segment"});
    }

    if (mCurrentActiveAlgorithmPtr->SupportsRegions()) {
        buttonsToCreate.push_back({ProcessAction<Mode::Segment>{}, "Regions"});
    }
    if (mCurrentActiveAlgorithmPtr->CreatesTakes()) {
        buttonsToCreate.push_back(
            {ProcessAction<Mode::ProcessAudio>{}, "Process"});
    }

    const long numActionButtons = buttonsToCreate.size();
    if (numActionButtons > 0) {
        for (int i = 0; i < numActionButtons; ++i) {
            const auto &buttonInfo = buttonsToCreate[i];
            IRECT b =
                actionButtonRowBounds
                    .GetGridCell(0, i, 1, static_cast<int>(numActionButtons))
                    .GetHPadded(buttonPadding);
            pGraphics->AttachControl(
                new ReacomaButton(b, buttonInfo.label, buttonInfo.function));
        }
    }

    const float cancelButtonWidth = 80.f;
    const float padding = 5.f;

    IRECT cancelBounds = bottomUtilityRowBounds.GetFromRight(cancelButtonWidth);
    IRECT progressBounds = bottomUtilityRowBounds;
    progressBounds.R = cancelBounds.L - padding;

    mProgressBar = new ReacomaProgressBar(progressBounds, "Overall Progress");
    pGraphics->AttachControl(mProgressBar);
    mProgressBar->SetDisabled(true);

    mCancelButton =
        new ReacomaButton(cancelBounds, "Cancel",
                          [this](IControl *pCaller) { CancelRunningJobs(); });
    pGraphics->AttachControl(mCancelButton);
    mCancelButton->SetDisabled(true);
}

void ReacomaExtension::Process(Mode mode, bool force) {
    mCurrentProcessingMode = mode;
    auto cores = std::thread::hardware_concurrency();
    mConcurrencyLimit = std::min(4U, cores);

    mPendingItemsQueue.clear();

    for (int i = 0; i < CountSelectedMediaItems(0); ++i) {
        mPendingItemsQueue.push_back(GetSelectedMediaItem(0, i));
    }

    mTotalBatchItems = mPendingItemsQueue.size();

    if (mPendingItemsQueue.empty() || mCurrentActiveAlgorithmPtr == nullptr) {
        return;
    }

    mIsProcessingBatch = true;
    mIsCancellationRequested = false;
    mLastReportedProgress = 0.0;
    mActiveJobs.clear();
    mFinalizationQueue.clear();

    if (mProgressBar) {
        mProgressBar->SetProgress(0.0);
        mProgressBar->SetDisabled(false);
    }
    if (mCancelButton) {
        mCancelButton->SetDisabled(false);
    }

    if (GetUI()) {
        IGraphics *pGraphics = GetUI();
        for (int i = 0; i < pGraphics->NControls(); ++i) {
            IControl *pControl = pGraphics->GetControl(i);
            if (pControl && pControl != mProgressBar &&
                pControl != mCancelButton) {
                pControl->SetDisabled(true);
            }
        }
    }

    mBatchUndoProject = GetItemProjectContext(mPendingItemsQueue.front());
    Undo_BeginBlock2(mBatchUndoProject);
}

void ReacomaExtension::OnParamChangeUI(int paramIdx, EParamSource source) {
    if (paramIdx == kParamAlgorithmChoice) {
        int selectedAlgoChoiceInt = GetParam(paramIdx)->Int();
        EAlgorithmChoice selectedAlgo =
            static_cast<EAlgorithmChoice>(selectedAlgoChoiceInt);

        if (selectedAlgo != mCurrentAlgorithmChoice) {
            SetAlgorithmChoice(selectedAlgo, true);
        }
    }
}

void ReacomaExtension::OnIdle() {
    if (!mIsProcessingBatch) {
        return;
    }

    if (mIsCancellationRequested) {
        for (auto &job : mActiveJobs) {
            job->Cancel();
            SetMediaItemInfo_Value(job->mItem, "C_LOCK", false);
        }

        mPendingItemsQueue.clear();
        mActiveJobs.clear();
        mFinalizationQueue.clear();

        mIsProcessingBatch = false;
        mIsCancellationRequested = false;

        Undo_EndBlock2(mBatchUndoProject, "Reacoma: Batch Process Cancelled",
                       -1);
        mBatchUndoProject = nullptr;

        ResetUIState();
        UpdateArrange();
        UpdateTimeline();
        return;
    }

    for (auto it = mActiveJobs.begin(); it != mActiveJobs.end();) {
        if ((*it)->IsFinished()) {
            mFinalizationQueue.push_back(std::move(*it));
            it = mActiveJobs.erase(it);
        } else {
            ++it;
        }
    }

    while (mActiveJobs.size() < mConcurrencyLimit &&
           !mPendingItemsQueue.empty()) {
        MediaItem *itemToProcess = mPendingItemsQueue.front();
        mPendingItemsQueue.pop_front();

        auto job =
            ProcessingJob::Create(mCurrentAlgorithmChoice, itemToProcess, this);
        if (job) {
            job->Start();
            mActiveJobs.push_back(std::move(job));
        }
    }

    if (!mFinalizationQueue.empty()) {
        auto &finishedJob = mFinalizationQueue.front();
        finishedJob->Finalize();
        mFinalizationQueue.pop_front();
    }

    if (mProgressBar && mTotalBatchItems > 0) {
        double totalProgressUnits = 0.0;

        for (const auto &job : mActiveJobs) {
            totalProgressUnits += job->GetProgress();
        }

        size_t completedJobs =
            mTotalBatchItems - mPendingItemsQueue.size() - mActiveJobs.size();
        totalProgressUnits += static_cast<double>(completedJobs);

        double overallProgress = totalProgressUnits / mTotalBatchItems;
        if (overallProgress > mLastReportedProgress) {
            mLastReportedProgress = overallProgress;
        }
        mProgressBar->SetProgress(mLastReportedProgress);
    }

    if (mPendingItemsQueue.empty() && mActiveJobs.empty() &&
        mFinalizationQueue.empty()) {
        mIsProcessingBatch = false;
        Undo_EndBlock2(mBatchUndoProject, "Reacoma: Process Batch", -1);
        mBatchUndoProject = nullptr;

        ResetUIState();
        UpdateArrange();
        UpdateTimeline();
    }
}

void ReacomaExtension::SetAlgorithmChoice(EAlgorithmChoice choice,
                                          bool triggerUIRelayout) {
    mCurrentAlgorithmChoice = choice;
    switch (choice) {
    case kNoveltySlice:
        mCurrentActiveAlgorithmPtr = mNoveltyAlgorithm.get();
        break;
    case kHPSS:
        mCurrentActiveAlgorithmPtr = mHPSSAlgorithm.get();
        break;
    case kNMF:
        mCurrentActiveAlgorithmPtr = mNMFAlgorithm.get();
        break;
    case kOnsetSlice:
        mCurrentActiveAlgorithmPtr = mOnsetSliceAlgorithm.get();
        break;
    case kTransientSlice:
        mCurrentActiveAlgorithmPtr = mTransientSliceAlgorithm.get();
        break;
    case kTransients:
        mCurrentActiveAlgorithmPtr = mTransientsAlgorithm.get();
        break;
    case kAmpGate:
        mCurrentActiveAlgorithmPtr = mAmpGateAlgorithm.get();
        break;
    default:
        mCurrentActiveAlgorithmPtr = nullptr;
        break;
    }
}

void ReacomaExtension::CancelRunningJobs() {
    if (mIsProcessingBatch) {
        mIsCancellationRequested = true;
        if (mCancelButton) {
            mCancelButton->SetDisabled(true);
        }
    }
}

void ReacomaExtension::ResetUIState() {
    if (GetUI()) {
        IGraphics *pGraphics = GetUI();
        for (int i = 0; i < pGraphics->NControls(); ++i) {
            IControl *pControl = pGraphics->GetControl(i);
            if (pControl) {
                pControl->SetDisabled(false);
            }
        }
    }

    if (mProgressBar) {
        mProgressBar->SetDisabled(true);
        mProgressBar->SetProgress(0.0);
    }

    if (mCancelButton) {
        mCancelButton->SetDisabled(true);
    }
}
