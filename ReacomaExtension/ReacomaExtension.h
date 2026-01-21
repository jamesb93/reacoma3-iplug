#pragma once

#include "ReaperExt_include_in_plug_hdr.h"
#include "reaper_plugin.h"
#include "IControl.h"
#include "IGraphicsStructs.h"

#include "ibmplexmono.hpp"
#include "roboto.hpp"

#include <chrono>
#include <list>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "Algorithms/IAlgorithm.h"
#include "JobManager.h"

namespace iplug {
namespace igraphics {
class ReacomaButton;
class ReacomaProgressBar;
class ReacomaSegmented;
class ReacomaParamTextControl;
} // namespace igraphics
} // namespace iplug

struct ReacomaTheme;

class IAlgorithm;
class ProcessingJob;

using namespace iplug;
using namespace igraphics;

class ReacomaExtension : public ReaperExtBase {

public:
    enum EParams { kParamAlgorithmChoice = 0, kNumOwnParams };

    enum EControlTags { kCtrlTagAlgoChooser = 0 };

    enum EAlgorithmChoice {
        kNoveltySlice = 0,
        kAmpSlice,
        kAmpGate,
        kOnsetSlice,
        kTransientSlice,
        kHPSS,
        kNMF,
        kTransients,
        kNumAlgorithmChoices
    };

    ReacomaExtension(reaper_plugin_info_t *pRec);
    void OnUIClose() override;
    void Process(ProcessingMode mode, bool force);
    void CancelRunningJobs();
    void ResetUIState();
    void SetAlgorithmChoice(EAlgorithmChoice choice, bool triggerUIRelayout);
    void UpdateAutoProcessButtonState();

private:
    bool mUIRelayoutIsNeeded = false;

    std::unique_ptr<ReacomaTheme> mTheme;
    std::unique_ptr<JobManager> mJobManager;

    std::vector<std::unique_ptr<IAlgorithm>> mAlgorithms;

    void OnParamChangeUI(int paramIdx, EParamSource source) override;
    void OnIdle() override;
    void SetupUI(IGraphics *pGraphics);
    void SetupAlgorithmSelector(IGraphics *pGraphics,
                                IRECT &currentLayoutBounds,
                                const ReacomaTheme &theme);
    void SetupAlgorithmParameters(IGraphics *pGraphics,
                                  IRECT &currentLayoutBounds,
                                  const ReacomaTheme &theme);
    void SetupActionButtons(IGraphics *pGraphics,
                            const IRECT &actionButtonRowBounds,
                            const ReacomaTheme &theme);
    void SetupFooterControls(IGraphics *pGraphics,
                             const IRECT &bottomUtilityRowBounds,
                             const ReacomaTheme &theme);
    void ManageProcessingJobs();
    void SaveState();
    void LoadState();
    std::string GetSettingsFilePath() const;

    int mGUIToggle = 0;

    IAlgorithm *mCurrentActiveAlgorithmPtr = nullptr;
    EAlgorithmChoice mCurrentAlgorithmChoice = kNoveltySlice;

    iplug::igraphics::ReacomaProgressBar *mProgressBar = nullptr;
    iplug::igraphics::ReacomaButton *mCancelButton = nullptr;
    iplug::igraphics::ReacomaButton *mAutoProcessButton = nullptr;

    // handle different processing modes
    bool mAutoProcessMode = false;
    bool mProcessIsPending = false;
    bool mHasUserInteractedSinceLoad = false;
    bool mStateLoaded = false;
    std::chrono::steady_clock::time_point mLastParamChangeTime;
    static constexpr auto AUTO_PROCESS_DELAY = std::chrono::milliseconds(50);
};