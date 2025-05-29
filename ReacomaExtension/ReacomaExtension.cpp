// ReacomaExtension.cpp
#define PLUG_CLASS_NAME ReacomaExtension
#define PLUG_WIDTH 500
#define PLUG_HEIGHT 480 // Adjusted height to potentially accommodate more controls
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0

#define SHARED_RESOURCES_SUBPATH "ReacomaExtension"
#include "ReacomaExtension.h"
#include "ReaperExt_include_in_plug_src.h"
//#include "ibmplexsans.hpp"
#include "ibmplexmono.hpp"
#include "IControls.h" // Make sure this includes ITextControl, ICheckboxControl if used
#include "ReacomaSlider.h"
#include "ReacomaButton.h"
#include "ReacomaSegmented.h"
#include "Algorithms/NoveltySliceAlgorithm.h"
#include "Algorithms/HPSSAlgorithm.h"
#include "IAlgorithm.h"
#include "IPlugParameter.h"

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
    mAlgorithm = std::make_unique<NoveltySliceAlgorithm>(this);
    if (mAlgorithm) {
        mAlgorithm->RegisterParameters();
    }
    
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

    mLayoutFunc = [&](IGraphics* pGraphics) {
        const IRECT bounds = pGraphics->GetBounds();
        const IColor swissBackgroundColor = COLOR_WHITE;

        if (pGraphics->NControls()) {
            IControl* pBG = pGraphics->GetBackgroundControl();
            if(pBG) {
                pBG->SetTargetAndDrawRECTs(bounds);
                return;
            }
        }
        pGraphics->EnableMouseOver(true);
        pGraphics->LoadFont("IBMPlexSans", (void*) IBMPLEXMONO, IBMPLEXMONO_length);
        pGraphics->AttachPanelBackground(swissBackgroundColor);

        float globalFramePadding = 20.f;
        float topContentMargin = 10.f;
        float bottomContentMargin = 10.f;
        float actionButtonHeight = 20.f;
        float buttonPadding = 1.f;
        float controlVisualHeight = 30.f; // Standard height for the visual part of the control
        float verticalSpacing = 5.f;   // Vertical spacing between controls

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

        if (mAlgorithm)
        {
            
            int numAlgoParams = mAlgorithm->GetNumAlgorithmParams();

            for (int i = 0; i < numAlgoParams; ++i)
            {
                if (currentLayoutBounds.H() < controlVisualHeight) break;

                int globalParamIdx = mAlgorithm->GetGlobalParamIdx(i);
                IParam* pParam = pGraphics->GetDelegate()->GetParam(globalParamIdx);
                
                // Allocate cell for the control + its name (optional, for future)
                IRECT controlCellRect = currentLayoutBounds.GetFromTop(controlVisualHeight);

                IParam::EParamType type = pParam->Type();
                auto paramName = pParam->GetName();

                if (type == IParam::EParamType::kTypeDouble || (type == IParam::EParamType::kTypeInt && pParam->GetMax() >= pParam->GetMin())) // Allow slider for single value Int too
                {
                    // Make slider rect a bit smaller than cell for padding, or use full cellRect
                    IRECT sliderBounds = controlCellRect.GetVPadded(-5.f); // Example padding
                    if (sliderBounds.H() < 10.f) sliderBounds.B = sliderBounds.T + 10.f; // Ensure min height

                    auto* slider = new ReacomaSlider(sliderBounds, globalParamIdx);
                    slider->SetTrackThickness(1.0f);
                    slider->SetHandleThickness(10.0f);
                    slider->SetDrawValue(true);
                    slider->SetTooltip(paramName); // Tooltip with param name
                    pGraphics->AttachControl(slider);
                }
                else if (type == IParam::EParamType::kTypeEnum && pParam->GetMax() > 0)
                {
                    std::vector<std::string> labels;
                    for (int val = 0; val < pParam->GetMax(); ++val)
                    {
                        const char* displayText = pParam->GetDisplayTextAtIdx(val);
                        labels.push_back(displayText);
                    }
                    
                    if (!labels.empty())
                    {
                        IText segmentedTextStyle(14.f, COLOR_BLACK, "IBMPlexSans");
                        IColor activeColor = DEFAULT_PRCOLOR;
                        IColor inactiveColor = DEFAULT_BGCOLOR;
                        // ReacomaSegmented uses paramIdx to get its value, which InitEnum maps correctly
                        pGraphics->AttachControl(new ReacomaSegmented(controlCellRect, globalParamIdx, labels, segmentedTextStyle, activeColor, inactiveColor));
                    }
                }
                
                currentLayoutBounds.T = controlCellRect.B;
                if (i < numAlgoParams - 1)
                {
                    if (currentLayoutBounds.H() < verticalSpacing) break;
                    currentLayoutBounds.T += verticalSpacing;
                }
            }
        }
        // Update the top of the remaining area for subsequent controls like action buttons
        mainContentArea.T = currentLayoutBounds.T;

        // Add a little space before action buttons if there were parameters
        if (mAlgorithm && mAlgorithm->GetNumAlgorithmParams() > 0) {
            if (mainContentArea.H() < 20.f) { /* Skip */ }
            else mainContentArea.T += 20.f;
        }
        
        // --- Action Buttons ---
        // Ensure there's enough height for the action button row
        if (mainContentArea.H() < actionButtonHeight) { /* Skip buttons */ }
        else {
            IRECT actionButtonRowBounds = mainContentArea.GetFromTop(actionButtonHeight);
            int numActionButtons = 3;
            auto AddReacomaAction = [&](IActionFunction function, int colIdx, const char* label) {
                IRECT b = actionButtonRowBounds.GetGridCell(0, colIdx, 1, numActionButtons)
                                .GetHPadded(buttonPadding);
                pGraphics->AttachControl(new ReacomaButton(b, label, function));
            };
            AddReacomaAction(ProcessAction<Mode::Segment>{}, 0, "Segment");
            AddReacomaAction(ProcessAction<Mode::Markers>{}, 1, "Markers");
            AddReacomaAction(ProcessAction<Mode::Regions>{}, 2, "Regions");
        }
    }; // End of mLayoutFunc
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
                success = mAlgorithm->ProcessItem(item);
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
    if (GetUI()) {
        // Potentially trigger redraw or other UI updates if needed
        // Example: GetUI()->SetControlDirty(paramIdx); // If you had a direct mapping
        // Or just redraw all:
        GetUI()->SetAllControlsDirty();
    }
}

void ReacomaExtension::OnIdle()
{
    if (GetUI() && !GetUI()->ControlIsCaptured())
    {
        // Idle tasks
    }
}
