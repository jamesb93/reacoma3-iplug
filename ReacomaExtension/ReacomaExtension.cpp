#include "ReacomaExtension.h"
#include "ReaperExt_include_in_plug_src.h"
#include "ibmplexsans.hpp"
#include "IControls.h"
#include "ReacomaSlider.h"
#include "ReacomaButton.h"
#include "ReacomaSegmented.h"
#include "Algorithms/NoveltySliceAlgorithm.h"
//#include "../../dependencies/flucoma-core/include/flucoma/clients/rt/NoveltySliceClient.hpp"
//#include "../../dependencies/flucoma-core/include/flucoma/clients/common/FluidContext.hpp"
//#include "../../dependencies/flucoma-core/include/flucoma/data/FluidMemory.hpp"


enum EAlgorithmOptions {
    kSpectrum = 0,
    kMFCC,
    kChroma,
    kPitch,
    kLoudness,
    kNumAlgorithmOptions
};

enum Params
{
    kThreshold = 0,
    kFiltersize,
    kAlgorithm,
    kNumParams
};

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
    for (int i = 0; i < kNumParams; i++) {
        AddParam();
    }

    GetParam(kThreshold)->InitDouble("Threshold", 0.5, 0.0, 1.0, 0.01);
    GetParam(kFiltersize)->InitInt("Filter Size", 3, 3, 100);
    
    GetParam(kAlgorithm)->InitInt("Algorithm", 0, 0, 4);
    GetParam(kAlgorithm)->SetDisplayText(EAlgorithmOptions::kSpectrum, "Spectrum");
    GetParam(kAlgorithm)->SetDisplayText(EAlgorithmOptions::kMFCC, "MFCC");
    GetParam(kAlgorithm)->SetDisplayText(EAlgorithmOptions::kChroma, "Chroma");
    GetParam(kAlgorithm)->SetDisplayText(EAlgorithmOptions::kPitch, "Pitch");
    GetParam(kAlgorithm)->SetDisplayText(EAlgorithmOptions::kLoudness, "Loudness");
    
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

        // Background panel setup (this is fine)
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

        // --- Layout Constants ---
        // These are your primary controls for overall padding and spacing
        float globalFramePadding = 20.f;  // Padding from all edges of the plugin window
        float topContentMargin = 10.f;    // Additional margin inside the top padded edge
        float bottomContentMargin = 20.f; // Space reserved at the bottom, inside the padded area

        // Spacing constants for between elements
        float sliderHeight = 10.f;
        float sliderRowCellHeight = sliderHeight + 0.f; // Includes space for potential labels above/below
        float spaceBetweenSlidersVertically = 0.f;
        float spaceBelowSliderStack = 0.f;
        float actionButtonHeight = 30.f;
        float buttonPadding = 2.f; // Padding for individual buttons, not global
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

        std::vector<int> sliderParamIndices;
        sliderParamIndices.push_back(kThreshold);
        sliderParamIndices.push_back(kFiltersize);

        if (!sliderParamIndices.empty())
        {
            for (size_t i = 0; i < sliderParamIndices.size(); ++i) {
                // Check remaining height before allocating space for the next slider cell
                if (currentLayoutBounds.H() < sliderRowCellHeight) break;

                if (i > 0) { // Apply vertical spacing *before* getting the rect for subsequent sliders
                    if (currentLayoutBounds.H() < (spaceBetweenSlidersVertically + sliderRowCellHeight)) break;
                    currentLayoutBounds.T += spaceBetweenSlidersVertically;
                }
                
                IRECT sliderCell = currentLayoutBounds.GetFromTop(sliderRowCellHeight);
                IRECT sliderActualBounds = sliderCell.GetMidVPadded(sliderHeight / 2.f);

                if (sliderActualBounds.W() < 10.f) sliderActualBounds.R = sliderActualBounds.L + 10.f; // Ensure min width
                 
                auto* reacomaSlider = new ReacomaSlider(sliderActualBounds, sliderParamIndices[i]);
                reacomaSlider->SetTrackThickness(2.0f);
                reacomaSlider->SetHandleThickness(10.0f);
                pGraphics->AttachControl(reacomaSlider);

                currentLayoutBounds.T = sliderCell.B; // Update top for the next element
            }
            // Space after the entire stack of sliders
            if (currentLayoutBounds.H() < spaceBelowSliderStack) { /* Potentially break or skip next sections */ }
            else currentLayoutBounds.T += spaceBelowSliderStack;
        }

        // --- Algorithm Segmented Control ---
        std::vector<std::string> segmentLabels = {"Spectrum", "MFCC", "Chroma", "Pitch", "Loudness"};
        IText segmentedTextStyle(14.f, COLOR_BLACK, "IBMPlexSans");
        IColor activeColor = DEFAULT_PRCOLOR;
        IColor inactiveColor = DEFAULT_BGCOLOR;

        // Check remaining height
        if (currentLayoutBounds.H() < (spaceBeforeSegmentedControl + segmentedControlHeight)) { /* Skip or handle */ }
        else {
            currentLayoutBounds.T += spaceBeforeSegmentedControl;
            if (currentLayoutBounds.H() < segmentedControlHeight) { /* Skip or handle */ }
            else {
                IRECT segmentedControlBounds = currentLayoutBounds.GetFromTop(segmentedControlHeight);
                pGraphics->AttachControl(new ReacomaSegmented(segmentedControlBounds,
                                                               kAlgorithm, // Using kAlgorithm param index
                                                               segmentLabels,
                                                               segmentedTextStyle,
                                                               activeColor,
                                                               inactiveColor
                                                               ));
                currentLayoutBounds.T = segmentedControlBounds.B;

                if (currentLayoutBounds.H() < spaceBelowSegmentedControl) { /* Skip or handle */ }
                else currentLayoutBounds.T += spaceBelowSegmentedControl;
            }
        }

        if (currentLayoutBounds.H() < actionButtonHeight) { /* Skip buttons */ }
        else {
            IRECT actionButtonRowBounds = currentLayoutBounds.GetFromTop(actionButtonHeight);
            int numActionButtons = 3;
            auto AddReacomaAction = [&](IActionFunction function, int colIdx, const char* label) {
                IRECT b = actionButtonRowBounds.GetGridCell(0, colIdx, 1, numActionButtons)
                              .GetHPadded(buttonPadding); // Padding for individual buttons
                pGraphics->AttachControl(new ReacomaButton(b, label, function));
            };
            AddReacomaAction(ProcessAction<Mode::Segment>{}, 0, "Segment");
            AddReacomaAction(ProcessAction<Mode::Markers>{}, 1, "Markers");
            AddReacomaAction(ProcessAction<Mode::Regions>{}, 2, "Regions");
            // currentLayoutBounds.T = actionButtonRowBounds.B; // No more controls after this in the snippet
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
//    auto mContext = new fluid::client::FluidContext();
//    auto mAllocator = new fluid::data::FluidDefaultAllocator();
//    auto alg = new NoveltySliceAlgorithm(this);
    auto alg = std::make_unique<NoveltySliceAlgorithm>(this);
    alg->ProcessItem(item);
//    return;
    // auto alg = new NoveltySliceAlgorithm();
    // alg->ProcessItem(item);
    // return;
    
//    std::vector<double> output;
//    MediaItem* item = GetSelectedMediaItem(0, 0);
//    MediaItem_Take* take = GetActiveTake(item);
//    PCM_source* source = GetMediaItemTake_Source(take);
//    int sampleRate = GetMediaSourceSampleRate(source);
//    int numChannels = GetMediaSourceNumChannels(source);
//    double itemLength = GetMediaItemInfo_Value(item, "D_LENGTH");
//    double playrate = GetMediaItemTakeInfo_Value(take, "D_PLAYRATE");
//    double takeOffset = GetMediaItemTakeInfo_Value(take, "D_STARTOFFS");
//
//    int outSize = sampleRate * std::min(itemLength * playrate, source->GetLength() - takeOffset);
//
//    output.resize(outSize);
//
//    PCM_source_transfer_t transfer;
//
//    transfer.time_s = takeOffset;
//    transfer.samplerate = sampleRate;
//    transfer.nch = 1;
//    transfer.length = outSize;
//    transfer.samples = output.data();
//    transfer.samples_out = 0;
//    transfer.midi_events = nullptr;
//    transfer.approximate_playback_latency = 0.0;
//    transfer.absolute_time_s = 0.0;
//    transfer.force_bpm = 0.0;
//
//    if (numChannels == 1) {
//        source->GetSamples(&transfer);
//    }
//    // Setup parameters
//    std::vector<float> output_float(output.size());
//    std::transform(output.begin(), output.end(), output_float.begin(),
//    [](double d) { return static_cast<float>(d); });
//    auto inputBuffer = InputBufferT::type(new fluid::VectorBufferAdaptor(output_float, numChannels, outSize, sampleRate));
//
//    int estimatedSlices = static_cast<int>(output.size() / 1024);
//    auto outBuffer = std::make_shared<MemoryBufferAdaptor>(1, estimatedSlices, sampleRate);
//    auto outputBuffer = BufferT::type(outBuffer);
//
//    auto threshold = GetParam(kThreshold)->Value();
//    auto algorithm = GetParam(kAlgorithm)->Value();
//
//    mParams.template set<0>(std::move(inputBuffer), nullptr);  // source buffer
//    mParams.template set<1>(LongT::type(0), nullptr);         // startFrame
//    mParams.template set<2>(LongT::type(-1), nullptr);        // numFrames (-1 = all)
//    mParams.template set<3>(LongT::type(0), nullptr);         // startChan
//    mParams.template set<4>(LongT::type(-1), nullptr);        // numChans (-1 = all)
//    mParams.template set<5>(std::move(outputBuffer), nullptr); // indices buffer
//    mParams.template set<6>(LongT::type(algorithm), nullptr);       // algorithm
//    mParams.template set<7>(LongRuntimeMaxParam(3, 3), nullptr); // kernelSize
//    mParams.template set<8>(FloatT::type(threshold), nullptr); // threshold
//    mParams.template set<9>(LongRuntimeMaxParam(3, 3), nullptr); // filterSize
//    mParams.template set<10>(LongT::type(2), nullptr);   // minSliceLength
//    mParams.template set<11>(fluid::client::FFTParams(1024, -1, -1), nullptr); // hopSize
//
//    mClient = NRTThreadingNoveltySliceClient(mParams, mContext);
//    mClient.setSynchronous(true);
//    mIsProcessingAsync = false;
//    mClient.enqueue(mParams);
//    Result result = mClient.process();
//
//    BufferAdaptor::ReadAccess reader(outputBuffer.get());
//    auto view = reader.samps(0);
//
//    Undo_BeginBlock2(0);
//
//    int markerCount = GetNumTakeMarkers(take);
//    for (int i = markerCount - 1; i >= 0; i--) {
//        DeleteTakeMarker(take, i);
//    }
//
//    int numMarkers = 0;
//    const char* markerLabel = "";
//
//    for (fluid::index i = 0; i < view.size(); i++) {
//        if (view(i) > 0) {
//            double markerTime = view(i) / sampleRate / playrate;
//            SetTakeMarker(take, -1, markerLabel, &markerTime, nullptr);
//            numMarkers++;
//        }
//    }
//
//    UpdateTimeline();
//    Undo_EndBlock2(0, "reacoma", -1);

    //    AudioAccessor* accessor = CreateTakeAudioAccessor(take);
    //    const int blockSize = 8192;
    //    std::vector<double> buffer(blockSize * numChannels);
    //    bool hasValidSamples = false;
    //
    //    for (int64_t sampleOffset=0; sampleOffset < numSamples; sampleOffset += blockSize) {
    //        int64_t remainingSamples = numSamples - sampleOffset;
    //        int samplesToRead = static_cast<int>(std::min<int64_t>(remainingSamples, blockSize));
    //        if (samplesToRead <= 0) {
    //            break;
    //        }
    //
    //        double position = (static_cast<double>(sampleOffset) / sampleRate);
    //        int ret = GetAudioAccessorSamples(
    //            accessor,
    //            sampleRate,
    //            numChannels,
    //            position,
    //            samplesToRead,
    //            buffer.data()
    //        );
    //
    //        if (ret == 1) {
    //            hasValidSamples = true;
    //            for (int sampleIdx = 0; sampleIdx < samplesToRead; sampleIdx++) {
    //                for (int chanIdx = 0; chanIdx < numChannels; chanIdx++) {
    //                    int sourceIdx = sampleIdx * numChannels + chanIdx;
    //                    int destIdx = chanIdx * numSamples + (sampleOffset + sampleIdx);
    //                    if (sourceIdx < buffer.size() && destIdx < m_audioData.size()) {
    //                        m_audioData[destIdx] = static_cast<float>(buffer[sourceIdx]);
    //                    }
    //                }
    //            }
    //        }
    //    }
    //    DestroyAudioAccessor(accessor);
    //
    //    if (!source->IsAvailable()) {
    //        return;
    //    }

    //    int numChannels = source->GetNumChannels();
    //    double sampleRate = source->GetSampleRate();
    //    int64_t numSamples = m_audioData.size() / numChannels;
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

    bool needsUpdate = false;

    return needsUpdate;
}

void ReacomaExtension::OnParamChangeUI(int paramIdx, EParamSource source)
{
    if (GetUI()) {
        //        Process(Mode::Segment, true);
    }
}

void ReacomaExtension::OnIdle()
{
    // Check if the UI is open and not currently being manipulated by a control
    if (GetUI() && !GetUI()->ControlIsCaptured())
    {
    }
}
