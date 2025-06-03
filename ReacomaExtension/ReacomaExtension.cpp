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
    // MODIFIED: This function now populates a queue with all selected items and starts the batch.

    if (mIsProcessingBatch) {
        ShowConsoleMsg("Reacoma: A batch is already being processed.\n");
        return;
    }

    const int numSelectedItems = CountSelectedMediaItems(0);
    if (numSelectedItems == 0) return;

    if (!mCurrentActiveAlgorithmPtr) return;

    // 1. Populate the queue with all selected items
    mProcessingQueue.clear();
    for (int i = 0; i < numSelectedItems; ++i)
    {
        mProcessingQueue.push_back(GetSelectedMediaItem(0, i));
    }
    
    mTotalQueueSize = mProcessingQueue.size();
    mQueueProgress = 0;

    if (!mProcessingQueue.empty())
    {
        // 2. Set the state for the entire batch operation
        mIsProcessingBatch = true;
        mBatchProcessingAlgorithm = mCurrentActiveAlgorithmPtr;
        
        // Disable UI controls here
        if (GetUI()) {
            // GetUI()->GetControlWithTag(kProcessButtonTag)->SetDisabled(true);
            GetUI()->SetAllControlsDirty();
        }

        // 3. Begin a single Undo block for the whole batch
        ReaProject* project = GetItemProjectContext(mProcessingQueue.front());
        Undo_BeginBlock2(project);

        // 4. Kick off the first item in the queue
        StartNextItemInQueue();
    }
}

void ReacomaExtension::StartNextItemInQueue()
{
    // MODIFIED: This new helper function is the heart of the queue logic.

    // Check if the queue is empty, which means the batch is complete.
    if (mProcessingQueue.empty())
    {
        ShowConsoleMsg("Reacoma: Batch processing complete.\n");
        
        // End the single Undo block
        ReaProject* project = GetItemProjectContext(mCurrentlyProcessingItem); // Use last item for context
        Undo_EndBlock2(project, "Reacoma: Process Batch", -1);

        // Reset all state
        mIsProcessingBatch = false;
        mBatchProcessingAlgorithm = nullptr;
        mCurrentlyProcessingItem = nullptr;
        mTotalQueueSize = 0;
        mQueueProgress = 0;

        // Re-enable UI controls
        if (GetUI()) {
            // GetUI()->GetControlWithTag(kProcessButtonTag)->SetDisabled(false);
            GetUI()->SetAllControlsDirty();
        }
        
        UpdateArrange();
        UpdateTimeline();
        return;
    }

    // Get the next item from the front of the queue
    mCurrentlyProcessingItem = mProcessingQueue.front();
    mProcessingQueue.pop_front();
    mQueueProgress++;

    // Start processing the current item
    if (mCurrentlyProcessingItem && mBatchProcessingAlgorithm)
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "Reacoma: Processing item %d of %d...\n", mQueueProgress, mTotalQueueSize);
        ShowConsoleMsg(msg);
        
        if (!mBatchProcessingAlgorithm->StartProcessItemAsync(mCurrentlyProcessingItem))
        {
            ShowConsoleMsg("Reacoma: Failed to start processing item. Skipping.\n");
            mCurrentlyProcessingItem = nullptr; // Clear the failed item
            StartNextItemInQueue(); // Immediately try to start the next one
        }
    }
}

bool ReacomaExtension::UpdateSelectedItems(bool force)
{
    const int numSelectedItems = CountSelectedMediaItems(0);
    std::vector<MediaItem*> selectedItems;
    for (int i = 0; i < numSelectedItems; i++)
        selectedItems.push_back(GetSelectedMediaItem(0, i));

    if (mSelectedItems != selectedItems)
    {
        std::swap(mSelectedItems, selectedItems);
        return true;
    }
    return false;
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
    // MODIFIED: This function now polls the *currently active item* and starts the next one upon completion.

    // If we're not running a batch or if there isn't an item actively processing, do nothing.
    if (!mIsProcessingBatch || !mCurrentlyProcessingItem || !mBatchProcessingAlgorithm) {
        return;
    }

    // Check if the currently active item has finished its background task.
    if (mBatchProcessingAlgorithm->IsFinished())
    {
        // Finalize the results for the completed item.
        bool success = mBatchProcessingAlgorithm->FinalizeProcess(mCurrentlyProcessingItem);
        if (!success) {
            ShowConsoleMsg("Reacoma: Finalization failed for an item.\n");
        }
        
        // The item is done, so clear the pointer.
        mCurrentlyProcessingItem = nullptr;

        // Immediately try to process the next item in the queue.
        StartNextItemInQueue();
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
