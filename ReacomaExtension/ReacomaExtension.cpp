#include "ReacomaExtension.h"

#define REAPERAPI_IMPLEMENT
#include "ReaperExt_include_in_plug_src.h"

#include "IControl.h"
#include "IControls.h"
#include "IGraphicsStructs.h"

#include <chrono>
#include <fstream>

#include "ReacomaTheme.h"
#include "Algorithms/AmpGateAlgorithm.h"
#include "Algorithms/AmpSliceAlgorithm.h"
#include "Algorithms/HPSSAlgorithm.h"
#include "Algorithms/NMFAlgorithm.h"
#include "Algorithms/NoveltySliceAlgorithm.h"
#include "Algorithms/OnsetSliceAlgorithm.h"
#include "Algorithms/ProcessingJob.h"
#include "Algorithms/TransientAlgorithm.h"
#include "Algorithms/TransientSliceAlgorithm.h"
#include "Components/ReacomaButton.h"
#include "Components/ReacomaParamTextControl.h"
#include "Components/ReacomaProgressBar.h"
#include "Components/ReacomaSegmented.h"

template <ProcessingMode M> struct ProcessAction {
    void operator()(IControl *pCaller) {
        static_cast<ReacomaExtension *>(pCaller->GetDelegate())
            ->Process(M, true);
    }
};

ReacomaExtension::ReacomaExtension(reaper_plugin_info_t *pRec)
    : ReaperExtBase(pRec) {

    mTheme = std::make_unique<ReacomaTheme>();
    mJobManager = std::make_unique<JobManager>();

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
    auto parameterLabels = {"Novelty Slice", "Amp Slice",       "Amp Gate",
                            "Onset Slice",   "Transient Slice", "HPSS",
                            "NMF",           "Transients"};
    GetParam(kParamAlgorithmChoice)
        ->InitEnum("Algorithm", kNoveltySlice, parameterLabels);

    // Initialize algorithms in the order defined by EAlgorithmChoice
    mAlgorithms.reserve(kNumAlgorithmChoices);
    mAlgorithms.push_back(std::make_unique<NoveltySliceAlgorithm>(this));
    mAlgorithms.push_back(std::make_unique<AmpSliceAlgorithm>(this));
    mAlgorithms.push_back(std::make_unique<AmpGateAlgorithm>(this));
    mAlgorithms.push_back(std::make_unique<OnsetSliceAlgorithm>(this));
    mAlgorithms.push_back(std::make_unique<TransientSliceAlgorithm>(this));
    mAlgorithms.push_back(std::make_unique<HPSSAlgorithm>(this));
    mAlgorithms.push_back(std::make_unique<NMFAlgorithm>(this));
    mAlgorithms.push_back(std::make_unique<TransientAlgorithm>(this));

    for (const auto &algo : mAlgorithms) {
        algo->SetBaseParamIdx(NParams());
        auto descriptors = algo->GetParamDescriptors();
        algo->InitParamValues(descriptors.size());
        for (const auto &d : descriptors) {
            IParam *p = AddParam();
            switch (d.type) {
                case ParameterDescriptor::Double:
                    p->InitDouble(d.name.c_str(), d.defaultVal, d.minVal,
                                  d.maxVal, d.step);
                    break;
                case ParameterDescriptor::Int:
                    p->InitInt(d.name.c_str(), static_cast<int>(d.defaultVal),
                               static_cast<int>(d.minVal),
                               static_cast<int>(d.maxVal));
                    break;
                case ParameterDescriptor::Enum:
                    p->InitEnum(d.name.c_str(), static_cast<int>(d.defaultVal),
                                static_cast<int>(d.enumLabels.size()));
                    for (int i = 0; i < d.enumLabels.size(); ++i) {
                        p->SetDisplayText(i, d.enumLabels[i].c_str());
                    }
                    break;
            }
        }
    }

    SetAlgorithmChoice(kNoveltySlice, false);

    mLayoutFunc = [&](IGraphics *pGraphics) { SetupUI(pGraphics); };
}

void ReacomaExtension::OnUIClose() {
    SaveState();
    mGUIToggle = 0;

    // Nullify pointers to destroyed controls to avoid use-after-free in OnIdle
    mProgressBar = nullptr;
    mCancelButton = nullptr;
    mAutoProcessButton = nullptr;
}

void ReacomaExtension::SetupUI(IGraphics *pGraphics) {
    // Clear previous controls
    pGraphics->RemoveAllControls();
    mProgressBar = nullptr;
    mCancelButton = nullptr;
    mAutoProcessButton = nullptr;
    mHasUserInteractedSinceLoad = false;
    mAutoProcessMode = false;

    const auto &theme = *mTheme; // Use the member theme object

    // Basic graphics setup
    pGraphics->EnableMouseOver(true);
    pGraphics->LoadFont("ibmplex", (void *)IBMPLEXMONO, IBMPLEXMONO_length);
    pGraphics->LoadFont("Roboto-Regular", (void *)ROBOTO_REGULAR,
                        ROBOTO_REGULAR_length);
    pGraphics->AttachPanelBackground(theme.bg);

    // --- Layout Constants ---
    const IRECT bounds = pGraphics->GetBounds();

    // --- Main Layout Areas ---
    IRECT mainContentArea = bounds.GetPadded(-theme.globalFramePadding);
    mainContentArea.T += 5.f; // Top margin
    mainContentArea.B -= 5.f; // Bottom margin

    IRECT remainingArea = mainContentArea;
    IRECT bottomUtilityRowBounds =
        remainingArea.GetFromBottom(theme.controlVisualHeight);
    remainingArea.B -= (bottomUtilityRowBounds.H() + theme.verticalSpacing);
    IRECT actionButtonRowBounds =
        remainingArea.GetFromBottom(theme.actionButtonHeight);
    remainingArea.B -= (actionButtonRowBounds.H() + theme.verticalSpacing);

    // Pass remainingArea by reference so it advances
    SetupAlgorithmSelector(pGraphics, remainingArea, theme);
    SetupAlgorithmParameters(pGraphics, remainingArea, theme);
    SetupActionButtons(pGraphics, actionButtonRowBounds, theme);
    SetupFooterControls(pGraphics, bottomUtilityRowBounds, theme);
}

void ReacomaExtension::SetupAlgorithmSelector(IGraphics *pGraphics,
                                              IRECT &currentLayoutBounds,
                                              const ReacomaTheme &theme) {
    if (currentLayoutBounds.H() >= theme.algoSelectorHeight) {
        IRECT algorithmSelectorRect =
            currentLayoutBounds.GetFromTop(theme.algoSelectorHeight);
        currentLayoutBounds.T = algorithmSelectorRect.B + theme.verticalSpacing;

        const IVStyle menuButtonStyle =
            DEFAULT_STYLE.WithColor(kFG, theme.inactive)
                .WithColor(kBG, theme.bg)
                .WithColor(kPR, theme.inactive)
                .WithLabelText(theme.buttonStyle)
                .WithValueText(theme.buttonStyle)
                .WithDrawShadows(false);

        auto *pAlgoChooser = new IVButtonControl(
            algorithmSelectorRect,
            [this, pGraphics](IControl *pCaller) {
                SplashClickActionFunc(pCaller);
                static IPopupMenu menu{""};

                menu.SetFunction([this, pCaller](IPopupMenu *pMenu) {
                    int itemIndex = pMenu->GetChosenItemIdx();
                    if (itemIndex > -1) {
                        GetParam(kParamAlgorithmChoice)->Set(itemIndex);
                        auto selectedAlgo =
                            static_cast<EAlgorithmChoice>(itemIndex);
                        this->SetAlgorithmChoice(selectedAlgo, true);
                    }
                });

                menu.Clear();
                IParam *pAlgoParam = GetParam(kParamAlgorithmChoice);
                for (int i = 0; i <= pAlgoParam->GetMax(); ++i) {
                    menu.AddItem(pAlgoParam->GetDisplayTextAtIdx(i));
                }

                float x, y;
                pGraphics->GetMouseDownPoint(x, y);
                pGraphics->CreatePopupMenu(*pCaller, menu, x, y);
            },
            "", menuButtonStyle, false, true);

        pGraphics->AttachControl(pAlgoChooser, kCtrlTagAlgoChooser,
                                 "vcontrols");

        IParam *pAlgoParam = GetParam(kParamAlgorithmChoice);
        pAlgoChooser->SetValueStr(
            pAlgoParam->GetDisplayTextAtIdx(mCurrentAlgorithmChoice));
    }
}

void ReacomaExtension::SetupAlgorithmParameters(IGraphics *pGraphics,
                                                IRECT &currentLayoutBounds,
                                                const ReacomaTheme &theme) {
    if (!mCurrentActiveAlgorithmPtr)
        return;

    int numAlgoParams = mCurrentActiveAlgorithmPtr->GetNumAlgorithmParams();
    for (int i = 0; i < numAlgoParams; ++i) {
        if (currentLayoutBounds.H() < theme.controlVisualHeight)
            break;

        int globalParamIdx = mCurrentActiveAlgorithmPtr->GetGlobalParamIdx(i);
        IParam *pParam = GetParam(globalParamIdx);
        IRECT controlCellRect =
            currentLayoutBounds.GetFromTop(theme.controlVisualHeight);

        if (pParam->Type() == IParam::kTypeDouble ||
            pParam->Type() == IParam::kTypeInt) {
            pGraphics->AttachControl(new ReacomaParamTextControl(
                controlCellRect.GetVPadded(-5.f), globalParamIdx, theme));
        } else if (pParam->Type() == IParam::kTypeEnum &&
                   pParam->GetMax() > 0) {
            std::vector<std::string> labels;
            for (int val = 0; val <= pParam->GetMax(); ++val)
                labels.push_back(pParam->GetDisplayTextAtIdx(val));

            if (!labels.empty())
                pGraphics->AttachControl(new ReacomaSegmented(
                    controlCellRect, globalParamIdx, labels, theme));
        }

        currentLayoutBounds.T = controlCellRect.B + theme.verticalSpacing;
    }
}

void ReacomaExtension::SetupActionButtons(IGraphics *pGraphics,
                                          const IRECT &actionButtonRowBounds,
                                          const ReacomaTheme &theme) {
    if (!mCurrentActiveAlgorithmPtr)
        return;

    struct ButtonInfo {
        IActionFunction function;
        const char *label;
    };
    std::vector<ButtonInfo> buttonsToCreate;

    if (mCurrentActiveAlgorithmPtr->SupportsSegmentation()) {
        buttonsToCreate.push_back(
            {ProcessAction<ProcessingMode::Segment>{}, "Segment"});
    }
    if (mCurrentActiveAlgorithmPtr->SupportsRegions()) {
        buttonsToCreate.push_back(
            {ProcessAction<ProcessingMode::Regions>{}, "Regions"});
    }
    if (mCurrentActiveAlgorithmPtr->CreatesTakes()) {
        buttonsToCreate.push_back(
            {ProcessAction<ProcessingMode::ProcessAudio>{}, "Process"});
    }

    const int numActionButtons = buttonsToCreate.size();
    if (numActionButtons > 0) {
        for (int i = 0; i < numActionButtons; ++i) {
            const auto &buttonInfo = buttonsToCreate[i];
            IRECT b =
                actionButtonRowBounds.GetGridCell(0, i, 1, numActionButtons)
                    .GetHPadded(-theme.padding);
            pGraphics->AttachControl(new iplug::igraphics::ReacomaButton(
                b, buttonInfo.label, buttonInfo.function, theme));
        }
    }
}

void ReacomaExtension::SetupFooterControls(IGraphics *pGraphics,
                                           const IRECT &bottomUtilityRowBounds,
                                           const ReacomaTheme &theme) {

    IRECT paddedBottomRow = bottomUtilityRowBounds.GetHPadded(-theme.padding);

    IRECT autoProcessBounds =
        paddedBottomRow.GetFromLeft(theme.autoProcessControlWidth);
    IRECT cancelBounds = paddedBottomRow.GetFromRight(theme.cancelButtonWidth);
    IRECT progressBounds = paddedBottomRow; // Start with the full padded width
    progressBounds.L = autoProcessBounds.R + theme.padding * 5;
    progressBounds.R = cancelBounds.L - theme.padding * 5;

    mAutoProcessButton = new iplug::igraphics::ReacomaButton(
        autoProcessBounds, "",
        [this](IControl *p) {
            mAutoProcessMode = !mAutoProcessMode;
            UpdateAutoProcessButtonState();
        },
        theme);
    pGraphics->AttachControl(mAutoProcessButton);
    UpdateAutoProcessButtonState();

    mProgressBar =
        new ReacomaProgressBar(progressBounds, "Overall Progress", theme);
    pGraphics->AttachControl(mProgressBar);
    mProgressBar->SetDisabled(true);

    mCancelButton = new ReacomaButton(
        cancelBounds, "Cancel", [this](IControl *p) { CancelRunningJobs(); },
        theme);
    pGraphics->AttachControl(mCancelButton);
    mCancelButton->SetDisabled(true);
}

void ReacomaExtension::UpdateAutoProcessButtonState() {
    if (mAutoProcessButton) {
        if (mAutoProcessMode) {
            mAutoProcessButton->SetLabel("Auto-Process: ON");
        } else {
            mAutoProcessButton->SetLabel("Auto-Process: OFF");
        }
        mAutoProcessButton->SetToggleState(mAutoProcessMode);
    }
}

void ReacomaExtension::Process(ProcessingMode mode, bool force) {
    if (!mCurrentActiveAlgorithmPtr) {
        return;
    }

    // Sync parameters before starting batch
    mCurrentActiveAlgorithmPtr->SyncParameters();

    std::vector<MediaItem *> itemsToProcess;
    for (int i = 0; i < CountSelectedMediaItems(0); ++i) {
        itemsToProcess.push_back(GetSelectedMediaItem(0, i));
    }

    if (itemsToProcess.empty()) {
        return;
    }

    mJobManager->StartBatch(itemsToProcess, mCurrentActiveAlgorithmPtr, mode);

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
}

void ReacomaExtension::OnParamChangeUI(int paramIdx, EParamSource source) {
    if (mAutoProcessMode && !mJobManager->IsProcessing() &&
        mHasUserInteractedSinceLoad) {
        mProcessIsPending = true;
        mLastParamChangeTime = std::chrono::steady_clock::now();
    }

    SaveState();

    mHasUserInteractedSinceLoad = true;
}

void ReacomaExtension::OnIdle() {

    if (!mStateLoaded) {
        LoadState();
        SetAlgorithmChoice(static_cast<EAlgorithmChoice>(
                               GetParam(kParamAlgorithmChoice)->Int()),
                           true);
        mStateLoaded = true;
    }

    if (mUIRelayoutIsNeeded && GetUI()) {
        SetupUI(GetUI());
        mUIRelayoutIsNeeded = false;
        IGraphics *pGraphics = GetUI();
        if (pGraphics) {
            IControl *pControl =
                pGraphics->GetControlWithTag(kCtrlTagAlgoChooser);
            if (pControl) {
                IVButtonControl *pAlgoButton = pControl->As<IVButtonControl>();
                IParam *pAlgoParam = GetParam(kParamAlgorithmChoice);
                if (pAlgoButton && pAlgoParam) {
                    WDL_String currentDisplayText;
                    pAlgoParam->GetDisplay(currentDisplayText);
                    pAlgoButton->SetValueStr(currentDisplayText.Get());
                }
            }
        }
    }

    if (mProcessIsPending && !mJobManager->IsProcessing()) {
        const auto currentTime = std::chrono::steady_clock::now();
        if (currentTime - mLastParamChangeTime > AUTO_PROCESS_DELAY) {
            mProcessIsPending = false;

            ProcessingMode modeToRun =
                mCurrentActiveAlgorithmPtr &&
                        mCurrentActiveAlgorithmPtr->CreatesTakes()
                    ? ProcessingMode::ProcessAudio
                    : ProcessingMode::Segment;

            Process(modeToRun, true);
        }
    }

    ManageProcessingJobs();
}

void ReacomaExtension::ManageProcessingJobs() {
    mJobManager->Tick();

    if (mJobManager->IsProcessing()) {
        if (mProgressBar) {
            mProgressBar->SetProgress(mJobManager->GetOverallProgress());
        }
    } else {
        ResetUIState();
    }
}

void ReacomaExtension::SetAlgorithmChoice(EAlgorithmChoice choice,
                                          bool triggerUIRelayout) {
    mCurrentAlgorithmChoice = choice;
    if (static_cast<size_t>(choice) < mAlgorithms.size()) {
        mCurrentActiveAlgorithmPtr =
            mAlgorithms[static_cast<size_t>(choice)].get();
    } else {
        mCurrentActiveAlgorithmPtr = nullptr;
    }

    if (triggerUIRelayout) {
        mUIRelayoutIsNeeded = true;
    }
}

void ReacomaExtension::CancelRunningJobs() {
    mJobManager->Cancel();
    if (mCancelButton) {
        mCancelButton->SetDisabled(true);
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

std::string ReacomaExtension::GetSettingsFilePath() const {
    const char *resourcePath = GetResourcePath();
    if (resourcePath && strlen(resourcePath) > 0) {
        std::string path(resourcePath);
        path += "/reacoma-settings.ini";
        return path;
    }
    return "";
}

void ReacomaExtension::SaveState() {
    std::string path = GetSettingsFilePath();
    if (path.empty())
        return;

    FILE *file = fopen(path.c_str(), "w");
    if (!file)
        return;

    // Save the global algorithm choice parameter
    IParam *pChoiceParam = GetParam(kParamAlgorithmChoice);
    if (pChoiceParam && pChoiceParam->GetName()) {
        fprintf(file, "%s=%f\n", pChoiceParam->GetName(),
                pChoiceParam->GetNormalized());
    }

    // Save all algorithm-specific parameters
    for (const auto &algoPtr : mAlgorithms) {
        IAlgorithm *pAlgorithm = algoPtr.get();
        if (!pAlgorithm || !pAlgorithm->GetName())
            continue;

        for (int i = 0; i < pAlgorithm->GetNumAlgorithmParams(); ++i) {
            int globalIdx = pAlgorithm->GetGlobalParamIdx(i);
            IParam *pParam = GetParam(globalIdx);

            if (!pParam || !pParam->GetName())
                continue;

            std::string algoNameStr = pAlgorithm->GetName();
            std::string paramNameStr = pParam->GetName();

            std::replace(algoNameStr.begin(), algoNameStr.end(), ' ', '_');
            std::replace(paramNameStr.begin(), paramNameStr.end(), ' ', '_');

            if (algoNameStr.empty() || paramNameStr.empty())
                continue;

            fprintf(file, "%s:%s=%f\n", algoNameStr.c_str(),
                    paramNameStr.c_str(), pParam->GetNormalized());
        }
    }
    fclose(file);
}

void ReacomaExtension::LoadState() {
    std::string path = GetSettingsFilePath();
    if (path.empty())
        return;

    std::ifstream settingsFile(path);
    if (!settingsFile.is_open())
        return;

    std::map<std::string, double> loadedSettings;
    std::string line;
    while (std::getline(settingsFile, line)) {
        auto pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string valueStr = line.substr(pos + 1);
            try {
                double value = std::stod(valueStr);
                loadedSettings[key] = value;
            } catch (const std::exception &e) {
                // Ignore parsing errors
            }
        }
    }

    // Load global algorithm choice
    IParam *pChoiceParam = GetParam(kParamAlgorithmChoice);
    if (pChoiceParam && pChoiceParam->GetName() &&
        loadedSettings.count(pChoiceParam->GetName())) {
        pChoiceParam->SetNormalized(loadedSettings[pChoiceParam->GetName()]);
    }

    // Load algorithm-specific parameters
    for (const auto &algoPtr : mAlgorithms) {
        IAlgorithm *pAlgorithm = algoPtr.get();
        if (!pAlgorithm || !pAlgorithm->GetName())
            continue;

        for (int i = 0; i < pAlgorithm->GetNumAlgorithmParams(); ++i) {
            int globalIdx = pAlgorithm->GetGlobalParamIdx(i);
            IParam *pParam = GetParam(globalIdx);
            if (!pParam || !pParam->GetName())
                continue;

            std::string algoName = pAlgorithm->GetName();
            std::string paramName = pParam->GetName();

            // Create the same safe key that was used for saving
            std::replace(algoName.begin(), algoName.end(), ' ', '_');
            std::replace(paramName.begin(), paramName.end(), ' ', '_');

            std::string key = algoName + ":" + paramName;
            if (loadedSettings.count(key)) {
                pParam->SetNormalized(loadedSettings[key]);
            }
        }
    }
}
