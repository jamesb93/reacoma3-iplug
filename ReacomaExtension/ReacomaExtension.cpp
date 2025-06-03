#define PLUG_CLASS_NAME ReacomaExtension
#define PLUG_WIDTH 500
#define PLUG_HEIGHT 480
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0

#define SHARED_RESOURCES_SUBPATH "ReacomaExtension"
#define REAPERAPI_IMPLEMENT

#include "ReacomaExtension.h"
#include "ReaperExt_include_in_plug_src.h"
#include "ibmplexmono.hpp"
#include "roboto.hpp"
#include "IControls.h"
#include "IAlgorithm.h"
#include "IPlugParameter.h"
#include "ReacomaSlider.h"
#include "ReacomaButton.h"
#include "ReacomaSegmented.h"
#include <deque>
#include <algorithm>

#include "Algorithms/ProcessingJob.h"

template <ReacomaExtension::Mode M>
struct ProcessAction
{
    void operator()(IControl* pCaller)
    {
        static_cast<ReacomaExtension*>(pCaller->GetDelegate())->Process(M, true);
    }
};

ReacomaExtension::ReacomaExtension(reaper_plugin_info_t* pRec) :
ReaperExtBase(pRec)
{
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

    RegisterAction("Reacoma: Show/Hide UI", [&]() { ShowHideMainWindow(); mGUIToggle = !mGUIToggle; }, true, &mGUIToggle);

    AddParam();
    GetParam(kParamAlgorithmChoice)->InitEnum("Algorithm", kNoveltySlice, kNumAlgorithmChoices);
    GetParam(kParamAlgorithmChoice)->SetDisplayText(kNoveltySlice, "Novelty Slice");
    GetParam(kParamAlgorithmChoice)->SetDisplayText(kHPSS, "HPSS");
    GetParam(kParamAlgorithmChoice)->SetDisplayText(kNMF, "NMF");

    mNoveltyAlgorithm = std::make_unique<NoveltySliceAlgorithm>(this);
    mNoveltyAlgorithm->RegisterParameters();

    mHPSSAlgorithm = std::make_unique<HPSSAlgorithm>(this);
    mHPSSAlgorithm->RegisterParameters();
    
    mNMFAlgorithm = std::make_unique<NMFAlgorithm>(this);
    mNMFAlgorithm->RegisterParameters();

    SetAlgorithmChoice(kNoveltySlice, false);

    mLayoutFunc = [&](IGraphics* pGraphics) {
        SetupUI(pGraphics);
    };
}

void ReacomaExtension::OnUIClose()
{
    mGUIToggle = 0;
}

void ReacomaExtension::SetupUI(IGraphics* pGraphics)
{
    const IRECT bounds = pGraphics->GetBounds();
    const IColor swissBackgroundColor = COLOR_WHITE;

    if (pGraphics->NControls()) {
        IControl* pBG = pGraphics->GetBackgroundControl();
        if(pBG) {
            pBG->SetTargetAndDrawRECTs(bounds);
        }
    }

    pGraphics->RemoveAllControls();

    pGraphics->EnableMouseOver(true);
    pGraphics->LoadFont("ibmplex", (void*) IBMPLEXMONO, IBMPLEXMONO_length);
    pGraphics->AttachPanelBackground(swissBackgroundColor);

    float globalFramePadding = 20.f;
    float topContentMargin = 10.f;
    float bottomContentMargin = 10.f;
    float actionButtonHeight = 30.f;
    float buttonPadding = 1.f;
    float controlVisualHeight = 30.f;
    float verticalSpacing = 10.f;

    IText segmentedTextStyle(14.f, COLOR_BLACK, "ibmplex");
    IColor activeColor = DEFAULT_PRCOLOR;
    IColor inactiveColor = DEFAULT_BGCOLOR;

    IRECT mainContentArea = bounds.GetPadded(-globalFramePadding);
    mainContentArea.T += topContentMargin;

    if (mainContentArea.H() > bottomContentMargin) {
        mainContentArea.B -= bottomContentMargin;
    } else {
        if (mainContentArea.T > mainContentArea.B) {
            mainContentArea.B = mainContentArea.T;
        }
    }

    IRECT currentLayoutBounds = mainContentArea;
    IRECT algorithmSelectorRect;
    if (currentLayoutBounds.H() >= controlVisualHeight) {
        algorithmSelectorRect = currentLayoutBounds.GetFromTop(controlVisualHeight);
        currentLayoutBounds.T = algorithmSelectorRect.B + verticalSpacing;
        std::vector<std::string> algoLabels;
        IParam* pAlgoChoiceParam = GetParam(kParamAlgorithmChoice);
        if (pAlgoChoiceParam) {
            for(int i = 0; i < pAlgoChoiceParam->GetMax() + 1; ++i) {
                algoLabels.push_back(pAlgoChoiceParam->GetDisplayTextAtIdx(i));
            }
        }
        pGraphics->AttachControl(new ReacomaSegmented(algorithmSelectorRect, kParamAlgorithmChoice, algoLabels, segmentedTextStyle, activeColor, inactiveColor));
    }

    if (!mCurrentActiveAlgorithmPtr) { return; }
    int numAlgoParams = mCurrentActiveAlgorithmPtr->GetNumAlgorithmParams();
    for (int i = 0; i < numAlgoParams; ++i)
    {
        if (currentLayoutBounds.H() < controlVisualHeight) break;

        int globalParamIdx = mCurrentActiveAlgorithmPtr->GetGlobalParamIdx(i);
        IParam* pParam = GetParam(globalParamIdx);

        IRECT controlCellRect = currentLayoutBounds.GetFromTop(controlVisualHeight);

        IParam::EParamType type = pParam->Type();
        auto paramName = pParam->GetName();

        if (type == IParam::EParamType::kTypeDouble || (type == IParam::EParamType::kTypeInt && pParam->GetMax() >= pParam->GetMin()))
        {
            IRECT sliderBounds = controlCellRect.GetVPadded(-5.f);
            if (sliderBounds.H() < 10.f) sliderBounds.B = sliderBounds.T + 10.f;

            auto* slider = new ReacomaSlider(sliderBounds, globalParamIdx);
            slider->SetTrackThickness(1.0f);
            slider->SetHandleThickness(10.0f);
            slider->SetDrawValue(true);
            slider->SetTooltip(paramName);
            pGraphics->AttachControl(slider);
        }
        else if (type == IParam::EParamType::kTypeEnum && pParam->GetMax() > 0)
        {
            std::vector<std::string> labels;
            for (int val = 0; val <= pParam->GetMax(); ++val)
            {
                const char* displayText = pParam->GetDisplayTextAtIdx(val);
                labels.push_back(displayText);
            }

            if (!labels.empty())
            {
                pGraphics->AttachControl(new ReacomaSegmented(controlCellRect, globalParamIdx, labels, segmentedTextStyle, activeColor, inactiveColor));
            }
        }

        currentLayoutBounds.T = controlCellRect.B + verticalSpacing;
    }

    mainContentArea.T = currentLayoutBounds.T;

    if (mainContentArea.H() >= actionButtonHeight) {
        IRECT actionButtonRowBounds = mainContentArea.GetFromTop(actionButtonHeight);
        int numActionButtons = 3;
        auto AddReacomaAction = [&](IActionFunction function, int colIdx, const char* label) {
            IRECT b = actionButtonRowBounds
                .GetGridCell(0, colIdx, 1, numActionButtons)
                .GetHPadded(buttonPadding);
            pGraphics->AttachControl(new ReacomaButton(b, label, function));
        };
        AddReacomaAction(ProcessAction<Mode::Segment>{}, 0, "Segment");
        AddReacomaAction(ProcessAction<Mode::Markers>{}, 1, "Markers");
        AddReacomaAction(ProcessAction<Mode::Regions>{}, 2, "Regions");
    }
}

void ReacomaExtension::Process(Mode mode, bool force)
{
    if (mIsProcessingBatch) {
        ShowConsoleMsg("Reacoma: A batch is already being processed.\n");
        return;
    }

    // 1. Determine concurrency limit. Default to 1 if detection fails.
    auto cores = std::thread::hardware_concurrency();
    mConcurrencyLimit = std::min(4U, cores);
    if (mConcurrencyLimit == 0) mConcurrencyLimit = 1;
    // 2. Populate the PENDING queue with all selected items
    mPendingItemsQueue.clear();
    for (int i = 0; i < CountSelectedMediaItems(0); ++i) {
        mPendingItemsQueue.push_back(GetSelectedMediaItem(0, i));
    }

    if (mPendingItemsQueue.empty() || mCurrentActiveAlgorithmPtr == nullptr) return;

    // 3. Set the state for the entire batch operation
    mIsProcessingBatch = true;
    mActiveJobs.clear();
    mFinalizationQueue.clear();
    
    if (GetUI()) { /* ... disable UI ... */ }

    mBatchUndoProject = GetItemProjectContext(mPendingItemsQueue.front());
    Undo_BeginBlock2(mBatchUndoProject);

    char msg[128];
    snprintf(msg, sizeof(msg), "Reacoma: Starting batch of %zu items with up to %u threads.\n", mPendingItemsQueue.size(), mConcurrencyLimit);
    ShowConsoleMsg(msg);
    
    // The OnIdle() loop will now handle starting jobs.
}

void ReacomaExtension::OnParamChangeUI(int paramIdx, EParamSource source)
{
    if (paramIdx == kParamAlgorithmChoice)
    {
        int selectedAlgoChoiceInt = GetParam(paramIdx)->Int();
        EAlgorithmChoice selectedAlgo = static_cast<EAlgorithmChoice>(selectedAlgoChoiceInt);

        if (selectedAlgo != mCurrentAlgorithmChoice)
        {
            SetAlgorithmChoice(selectedAlgo, true);
        }
    }
    else
    {
    }
}

void ReacomaExtension::OnIdle()
{
    if (!mIsProcessingBatch) {
        return;
    }

    // --- Phase 1: Check for finished jobs in the active list ---
    // Move any completed jobs to the finalization queue.
    for (auto it = mActiveJobs.begin(); it != mActiveJobs.end(); )
    {
        if ((*it)->IsFinished())
        {
            mFinalizationQueue.push_back(std::move(*it));
            it = mActiveJobs.erase(it); // A slot has been freed up.
        }
        else
        {
            ++it;
        }
    }

    // --- Phase 2: Start new jobs from the pending queue if there are free slots ---
    while (mActiveJobs.size() < mConcurrencyLimit && !mPendingItemsQueue.empty())
    {
        // Get the next item to process
        MediaItem* itemToProcess = mPendingItemsQueue.front();
        mPendingItemsQueue.pop_front();

        // Create and start a new job
        auto job = ProcessingJob::Create(mCurrentAlgorithmChoice, itemToProcess, this);
        if (job) {
            job->Start();
            mActiveJobs.push_back(std::move(job)); // Add it to the active list
        }
    }

    // --- Phase 3: Process one finished job from the finalization queue (main thread safe) ---
    if (!mFinalizationQueue.empty())
    {
        auto& finishedJob = mFinalizationQueue.front();
        finishedJob->Finalize();
        mFinalizationQueue.pop_front();
    }

    // --- Phase 4: Check if the entire batch is complete ---
    // This happens when there are no more pending items and no more active jobs.
    if (mPendingItemsQueue.empty() && mActiveJobs.empty() && mFinalizationQueue.empty())
    {
        mIsProcessingBatch = false;
        ShowConsoleMsg("Reacoma: Batch processing complete.\n");
        
        Undo_EndBlock2(mBatchUndoProject, "Reacoma: Process Batch", -1);
        mBatchUndoProject = nullptr;

        if (GetUI()) { /* ... re-enable UI ... */ }
        
        UpdateArrange();
        UpdateTimeline();
    }
}

void ReacomaExtension::SetAlgorithmChoice(EAlgorithmChoice choice, bool triggerUIRelayout)
{
    mCurrentAlgorithmChoice = choice;
    switch (choice)
    {
        case kNoveltySlice:
            mCurrentActiveAlgorithmPtr = mNoveltyAlgorithm.get();
            break;
        case kHPSS:
            mCurrentActiveAlgorithmPtr = mHPSSAlgorithm.get();
            break;
        case kNMF:
            mCurrentActiveAlgorithmPtr = mNMFAlgorithm.get();
            break;
        default:
            mCurrentActiveAlgorithmPtr = nullptr;
            break;
    }
}
