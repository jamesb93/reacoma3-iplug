// ReacomaExtension.cpp
#define PLUG_CLASS_NAME ReacomaExtension
#define PLUG_WIDTH 500
#define PLUG_HEIGHT 480 // Adjusted height to potentially accommodate more controls
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0

#define SHARED_RESOURCES_SUBPATH "ReacomaExtension"
#include "ReacomaExtension.h"
#include "ReaperExt_include_in_plug_src.h"
#include "ibmplexmono.hpp"
#include "roboto.hpp"
#include "IControls.h" // Make sure this includes ITextControl, ICheckboxControl if used
#include "Algorithms/NoveltySliceAlgorithm.h"
#include "Algorithms/HPSSAlgorithm.h"
#include "IAlgorithm.h"
#include "IPlugParameter.h"
#include "ReacomaSlider.h"
#include "ReacomaButton.h"
#include "ReacomaSegmented.h"

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
    IMPAPI(ValidatePtr2);
    IMPAPI(UpdateArrange);
    IMPAPI(UpdateTimeline);

    mMakeGraphicsFunc = [&]() {
        return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS);
    };

    RegisterAction("Reacoma: Show/Hide UI", [&]() { ShowHideMainWindow(); mGUIToggle = !mGUIToggle; }, true, &mGUIToggle);
    
    AddParam();
    GetParam(kParamAlgorithmChoice)->InitEnum("Algorithm", kNoveltySlice, kNumAlgorithmChoices - 1);
    GetParam(kParamAlgorithmChoice)->SetDisplayText(kNoveltySlice, "Novelty Slice");
    GetParam(kParamAlgorithmChoice)->SetDisplayText(kHPSS, "HPSS");

    mNoveltyAlgorithm = std::make_unique<NoveltySliceAlgorithm>(this);
    mNoveltyAlgorithm->RegisterParameters();

    mHPSSAlgorithm = std::make_unique<HPSSAlgorithm>(this);
    mHPSSAlgorithm->RegisterParameters();

    SetAlgorithmChoice(kNoveltySlice, false);

    mLayoutFunc = [&](IGraphics* pGraphics) {
        pGraphics->SetLayoutOnResize(true);
        const IRECT bounds = pGraphics->GetBounds();
        const IColor swissBackgroundColor = COLOR_WHITE;

        if (pGraphics->NControls()) { // You might need a flag like mForceLayout
                                                     // set by SetAlgorithmChoice to force full redraw
            IControl* pBG = pGraphics->GetBackgroundControl();
            if(pBG) {
                pBG->SetTargetAndDrawRECTs(bounds);
            }
        }
        
        pGraphics->RemoveAllControls(); // Or selectively remove algorithm-specific controls

        pGraphics->EnableMouseOver(true);
        pGraphics->LoadFont("ibmplex", (void*) IBMPLEXMONO, IBMPLEXMONO_length);
        // pGraphics->LoadFont("Roboto-Regular", (void*) ROBOTO_REGULAR, ROBOTO_REGULAR_length); // Already loaded
        pGraphics->AttachPanelBackground(swissBackgroundColor);
            
        float globalFramePadding = 20.f;
        float topContentMargin = 10.f;
        float bottomContentMargin = 10.f;
        float actionButtonHeight = 30.f; // Increased for better clickability
        float buttonPadding = 1.f;
        float controlVisualHeight = 30.f;
        float verticalSpacing = 10.f;   // Increased for clarity
            
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
        if (currentLayoutBounds.H() >= controlVisualHeight) { // Check if there's space
            algorithmSelectorRect = currentLayoutBounds.GetFromTop(controlVisualHeight); // Use proper height
            currentLayoutBounds.T = algorithmSelectorRect.B + verticalSpacing; // IMPORTANT: Advance currentLayoutBounds.T

            std::vector<std::string> algoLabels;
            IParam* pAlgoChoiceParam = GetParam(kParamAlgorithmChoice);
            if (pAlgoChoiceParam) { // Check if param exists
                for(int i = 0; i < pAlgoChoiceParam->GetMax() + 1; ++i) { // GetMax() is max index, so loop to GetMax() + 1 for count
                    algoLabels.push_back(pAlgoChoiceParam->GetDisplayTextAtIdx(i));
                }
            }
            pGraphics->AttachControl(new ReacomaSegmented(algorithmSelectorRect, kParamAlgorithmChoice, algoLabels, segmentedTextStyle, activeColor, inactiveColor));
        }
            
        if (mCurrentActiveAlgorithmPtr && mForceParameterRedraw)
        {
            mForceParameterRedraw = false;
            int numAlgoParams = mCurrentActiveAlgorithmPtr->GetNumAlgorithmParams();
            for (int i = 0; i < numAlgoParams; ++i)
            {
                if (currentLayoutBounds.H() < controlVisualHeight) break; // Check space against remaining currentLayoutBounds

                int globalParamIdx = mCurrentActiveAlgorithmPtr->GetGlobalParamIdx(i);
                IParam* pParam = GetParam(globalParamIdx); // No need for pGraphics->GetDelegate()->
                
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
                else if (type == IParam::EParamType::kTypeEnum && pParam->GetMax() > 0) // pParam->GetMax() for enums is count-1
                {
                    std::vector<std::string> labels;
                    for (int val = 0; val <= pParam->GetMax(); ++val) // Loop up to and including GetMax()
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
        }
        
        mainContentArea.T = currentLayoutBounds.T;

        if (mainContentArea.H() >= actionButtonHeight) { // Check for space for buttons
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
    };
}

void ReacomaExtension::OnUIClose()
{
    mGUIToggle = 0;
}

void ReacomaExtension::Process(Mode mode, bool force)
{
    const int numSelectedItems = CountSelectedMediaItems(0);
    if (numSelectedItems == 0) {
        return;
    }

    ReaProject* project = GetItemProjectContext(GetSelectedMediaItem(0, 0));

    bool undoBlockStarted = false;
    const char* undoDesc = "Reacoma Process"; // Default undo description

    if (mode != Mode::Preview) // Assuming a Preview mode might be added later
    {
        switch (mode) {
            case Mode::Segment: undoDesc = "Reacoma: Segment Item(s)"; break;
            case Mode::Markers: undoDesc = "Reacoma: Process Markers for Item(s)"; break;
            case Mode::Regions: undoDesc = "Reacoma: Create Region(s) for Item(s)"; break;
            default: undoDesc = "Reacoma: Unknown Action"; break;
        }
        Undo_BeginBlock2(project);
        undoBlockStarted = true;
    }

    for (int i = 0; i < numSelectedItems; ++i)
    {
        MediaItem* item = GetSelectedMediaItem(0, i);
        if (!item) continue;

        bool success = false;
        switch (mode)
        {
            case Mode::Segment:
                ShowConsoleMsg("Segment mode is conceptual. Implement in IAlgorithm if needed.\n");
                break;
            case Mode::Markers:
                success = mCurrentActiveAlgorithmPtr->ProcessItem(item);
                break;
            case Mode::Regions:
                ShowConsoleMsg("Regions mode is conceptual. Implement in IAlgorithm if needed.\n");
                break;
            case Mode::Preview: // If preview functionality is added later
                // mAlgorithm->UpdatePreviewForItem(item); // Hypothetical
                break;
            default:
                ShowConsoleMsg("ReacomaExtension: Unknown processing mode.\n");
                break;
        }
        // You might want to log success/failure or handle errors per item.
    }

    if (undoBlockStarted)
    {
        Undo_EndBlock2(project, undoDesc, -1); // -1 for default flags
    }

    // After processing, update REAPER's arrange view and timeline if necessary
    if (mode != Mode::Preview) {
        UpdateArrange();  // Redraws the arrange view
        UpdateTimeline(); // Redraws the timeline
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
    // bool needsUpdate = false; // This was unused, simplified
    return false;
}

void ReacomaExtension::OnParamChangeUI(int paramIdx, EParamSource source)
{
    if (paramIdx == kParamAlgorithmChoice) // kParamAlgorithmChoice is the index of your algo selector param
    {
        int selectedAlgoChoiceInt = GetParam(paramIdx)->Int();
        EAlgorithmChoice selectedAlgo = static_cast<EAlgorithmChoice>(selectedAlgoChoiceInt);

        if (selectedAlgo != mCurrentAlgorithmChoice)
        {
            SetAlgorithmChoice(selectedAlgo, true); // true to trigger UI relayout
        }
    }
    else
    {
        // If the change is for a parameter belonging to the currently active algorithm,
        // you might want to update its control or the whole UI.
//        if (GetUI()) {
//             GetUI()->SetAllControlsDirty(); // Or be more specific to the control linked to paramIdx
//        }
    }
}

void ReacomaExtension::OnIdle()
{
    if (GetUI() && !GetUI()->ControlIsCaptured())
    {
        // Idle tasks
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
        default:
            mCurrentActiveAlgorithmPtr = nullptr; // Should not happen
            break;
    }
    
    mForceParameterRedraw = true;

    if (triggerUIRelayout && GetUI())
    {
        // Force the UI to re-layout by calling Resize.
        // This will typically trigger your mLayoutFunc.
        GetUI()->Resize(PLUG_WIDTH, PLUG_HEIGHT, 1.0);
    }
    // If parameters were being dynamically added/removed (not the current plan),
    // you would call mCurrentActiveAlgorithmPtr->RegisterParameters() here after clearing old ones.
}
