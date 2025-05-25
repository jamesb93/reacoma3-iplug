// ReacomaExtension.cpp
#define PLUG_CLASS_NAME ReacomaExtension
#define PLUG_WIDTH 500
#define PLUG_HEIGHT 480
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0

#define ROBOTO_FN "IBMPlexSans.ttf"
#define SHARED_RESOURCES_SUBPATH "ReacomaExtension"
#include "ReacomaExtension.h"
#include "ReaperExt_include_in_plug_src.h"
#include "ibmplexsans.hpp"
#include "IControls.h"
#include "ReacomaSlider.h"
#include "ReacomaButton.h"
#include "ReacomaSegmented.h"
#include "Algorithms/NoveltySliceAlgorithm.h" // Make sure this is correctly pathed
#include "IAlgorithm.h" // Include IAlgorithm

// Removed old global Params and EAlgorithmOptions enums

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
    // Create and register algorithm and its parameters
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
        const IRECT bounds = pGraphics->GetBounds(); // Full plugin window area
        const IColor swissBackgroundColor = COLOR_WHITE;

        if (pGraphics->NControls()) {
            IControl* pBG = pGraphics->GetBackgroundControl();
            if(pBG) {
                pBG->SetTargetAndDrawRECTs(bounds);
                return;
            }
        }
        pGraphics->EnableMouseOver(true);
        pGraphics->LoadFont("IBMPlexSans", (void*) IBMPLEXSANS, IBMPLEXSANS_length);
        pGraphics->AttachPanelBackground(swissBackgroundColor);

        float globalFramePadding = 20.f;
        float topContentMargin = 10.f;
        float bottomContentMargin = 20.f;

        float sliderHeight = 10.f;
        float sliderRowCellHeight = sliderHeight + 0.f;
        float spaceBetweenSlidersVertically = 0.f;
        float spaceBelowSliderStack = 0.f;
        float actionButtonHeight = 30.f;
        float buttonPadding = 2.f;
        float segmentedControlHeight = 30.f;
        float spaceBeforeSegmentedControl = 15.f;
        float spaceBelowSegmentedControl = 20.f;

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

        // Use mAlgorithm to get correct parameter indices
        if (mAlgorithm)
        {
            std::vector<int> sliderParamIndices;
            // Cast mAlgorithm to NoveltySliceAlgorithm* to access its specific enums
            // This is safe if you know mAlgorithm is always a NoveltySliceAlgorithm here.
            // For a more generic approach, you might need a different way to identify params.
            auto* noveltyAlgo = dynamic_cast<NoveltySliceAlgorithm*>(mAlgorithm.get());
            if (noveltyAlgo) {
                 sliderParamIndices.push_back(mAlgorithm->GetGlobalParamIdx(NoveltySliceAlgorithm::kThreshold));
                 sliderParamIndices.push_back(mAlgorithm->GetGlobalParamIdx(NoveltySliceAlgorithm::kFilterSize));
            }


            if (!sliderParamIndices.empty())
            {
                for (size_t i = 0; i < sliderParamIndices.size(); ++i) {
                    if (currentLayoutBounds.H() < sliderRowCellHeight) break;

                    if (i > 0) {
                        if (currentLayoutBounds.H() < (spaceBetweenSlidersVertically + sliderRowCellHeight)) break;
                        currentLayoutBounds.T += spaceBetweenSlidersVertically;
                    }
                    
                    IRECT sliderCell = currentLayoutBounds.GetFromTop(sliderRowCellHeight);
                    IRECT sliderActualBounds = sliderCell.GetMidVPadded(sliderHeight / 2.f);

                    if (sliderActualBounds.W() < 10.f) sliderActualBounds.R = sliderActualBounds.L + 10.f;
                        
                    auto* reacomaSlider = new ReacomaSlider(sliderActualBounds, sliderParamIndices[i]);
                    reacomaSlider->SetTrackThickness(2.0f);
                    reacomaSlider->SetHandleThickness(10.0f);
                    pGraphics->AttachControl(reacomaSlider);

                    currentLayoutBounds.T = sliderCell.B;
                }
                if (currentLayoutBounds.H() < spaceBelowSliderStack) { /* Potentially break */ }
                else currentLayoutBounds.T += spaceBelowSliderStack;
            }

            // --- Algorithm Segmented Control ---
            std::vector<std::string> segmentLabels;
            // Populate labels based on NoveltySliceAlgorithm::EAlgorithmOptions
            if (noveltyAlgo) {
                for(int i = 0; i < NoveltySliceAlgorithm::kNumAlgorithmOptions; ++i) {
                    // This assumes IParam::GetDisplayText can be used, or you manually map enum to string
                    // For simplicity, using the same strings as before.
                    // You might need to retrieve these from IParam if they are set there.
                     switch(i) {
                        case NoveltySliceAlgorithm::kSpectrum: segmentLabels.push_back("Spectrum"); break;
                        case NoveltySliceAlgorithm::kMFCC: segmentLabels.push_back("MFCC"); break;
                        case NoveltySliceAlgorithm::kChroma: segmentLabels.push_back("Chroma"); break;
                        case NoveltySliceAlgorithm::kPitch: segmentLabels.push_back("Pitch"); break;
                        case NoveltySliceAlgorithm::kLoudness: segmentLabels.push_back("Loudness"); break;
                     }
                }
            }


            IText segmentedTextStyle(14.f, COLOR_BLACK, "IBMPlexSans");
            IColor activeColor = DEFAULT_PRCOLOR;
            IColor inactiveColor = DEFAULT_BGCOLOR;

            if (currentLayoutBounds.H() < (spaceBeforeSegmentedControl + segmentedControlHeight)) { /* Skip */ }
            else {
                currentLayoutBounds.T += spaceBeforeSegmentedControl;
                if (currentLayoutBounds.H() < segmentedControlHeight) { /* Skip */ }
                else {
                    IRECT segmentedControlBounds = currentLayoutBounds.GetFromTop(segmentedControlHeight);
                    if (noveltyAlgo && !segmentLabels.empty()) { // Ensure labels are populated
                        pGraphics->AttachControl(new ReacomaSegmented(segmentedControlBounds,
                                                                  mAlgorithm->GetGlobalParamIdx(NoveltySliceAlgorithm::kAlgorithm),
                                                                  segmentLabels,
                                                                  segmentedTextStyle,
                                                                  activeColor,
                                                                  inactiveColor
                                                                  ));
                    }
                    currentLayoutBounds.T = segmentedControlBounds.B;

                    if (currentLayoutBounds.H() < spaceBelowSegmentedControl) { /* Skip */ }
                    else currentLayoutBounds.T += spaceBelowSegmentedControl;
                }
            }
        }


        if (currentLayoutBounds.H() < actionButtonHeight) { /* Skip buttons */ }
        else {
            IRECT actionButtonRowBounds = currentLayoutBounds.GetFromTop(actionButtonHeight);
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
    MediaItem* item = GetSelectedMediaItem(0, 0);
    
    if (mAlgorithm && item) {
        // Assuming the mode implies which algorithm or action to take.
        // For now, we only have one algorithm.
        // If 'mode' was meant to select different algorithms, that logic would go here.
        if (mode == Mode::Markers) { // Or Segment, Regions if they use the same algo
             mAlgorithm->ProcessItem(item);
        }
    }
    // Old code removed for clarity
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
