#include "ReacomaSlider.h"
#include "IPlugParameter.h" // For GetParam()->GetDisplayForHost()
#include "IGraphics.h"       // For GetUI()->CreateTextEntry()
#include "IGraphicsConstants.h" // For colors like COLOR_GRAY, etc.
#include "IPlugUtilities.h"   // For Clip, Lerp

#include <cmath>
#include <algorithm>

namespace iplug {
namespace igraphics {

// Define default colors if they are not globally accessible or part of a theme
const IColor SWISS_TRACK_BG_COLOR_DEFAULT = COLOR_LIGHT_GRAY.WithOpacity(0.5f);
const IColor SWISS_TRACK_FILL_COLOR_DEFAULT = COLOR_BLACK;
const IColor SWISS_HANDLE_COLOR_BASE_DEFAULT = COLOR_RED;
const IColor SWISS_HANDLE_COLOR_HOVER_DEFAULT = COLOR_RED.WithContrast(0.2f);
const IColor SWISS_HANDLE_COLOR_DRAG_DEFAULT = COLOR_RED.WithContrast(0.4f);

ReacomaSlider::ReacomaSlider(const IRECT& bounds, int paramIdx)
    : IControl(bounds, paramIdx),
      mTrackBackgroundColor(SWISS_TRACK_BG_COLOR_DEFAULT),
      mTrackFillColor(SWISS_TRACK_FILL_COLOR_DEFAULT),
      mSliderHandleColor(SWISS_HANDLE_COLOR_BASE_DEFAULT),
      mMouseOverHandleColor(SWISS_HANDLE_COLOR_HOVER_DEFAULT),
      mDragHandleColor(SWISS_HANDLE_COLOR_DRAG_DEFAULT),
      mBaseHandleThickness(10.f),
      mTrackThickness(1.f),
      mHoverHandleThickness(12.f),
      mDragHandleThickness(14.f),
      mCurrentHandleThickness(mBaseHandleThickness),
      mCurrentSliderHandleColor(mSliderHandleColor),
      mIsDragging(false),
      mAnimationDurationMs(100),
      mAnimationTargetState(EAnimationState::kNone),
      mValueTextStyle(12.f, SWISS_TRACK_FILL_COLOR_DEFAULT, "ibmplex", EAlign::Center, EVAlign::Middle),
      mDrawValue(true),
      mValueTextPadding(2.f),
      mReservedTextWidth(40.f),
      mReservedTextHeight(18.f)
{
    SetTooltip("Reacoma Slider");
    mValueTextStyle.mFGColor = mTrackFillColor; // Initial text color based on track fill
    CalculateValueTextRect();                   // Initial calculation of text rect
}

void ReacomaSlider::SetSwissColors(const IColor& trackBgColor, const IColor& trackFillColor, const IColor& handleColor, const IColor& hoverColor, const IColor& dragColor)
{
    mTrackBackgroundColor = trackBgColor;
    mTrackFillColor = trackFillColor;
    mSliderHandleColor = handleColor;
    mMouseOverHandleColor = hoverColor;
    mDragHandleColor = dragColor;

    mValueTextStyle.mFGColor = mTrackFillColor;
    if (!GetAnimationFunction()) {
        if (mIsDragging)
            mCurrentSliderHandleColor = mDragHandleColor;
        else if (mMouseIsOver)
            mCurrentSliderHandleColor = mMouseOverHandleColor;
        else
            mCurrentSliderHandleColor = mSliderHandleColor;
    }
    SetDirty(false);
}

void ReacomaSlider::SetHandleThickness(float thickness)
{
    const auto defaultHoverFactor = 1.2f;
    const auto defaultDragFactor = 1.4f;

    mBaseHandleThickness = thickness;
    mHoverHandleThickness = thickness * defaultHoverFactor;
    mDragHandleThickness = thickness * defaultDragFactor;

    // Update current handle thickness based on state (if not animating)
    if (!GetAnimationFunction()) {
        if (mIsDragging)
            mCurrentHandleThickness = mDragHandleThickness;
        else if (mMouseIsOver)
            mCurrentHandleThickness = mHoverHandleThickness;
        else
            mCurrentHandleThickness = mBaseHandleThickness;
    }
    SetDirty(false);
}

void ReacomaSlider::SetTrackFillColor(const IColor& color)
{
    mTrackFillColor = color;
    mValueTextStyle.mFGColor = color; // Update text color to match
    SetDirty(false);
}

void ReacomaSlider::CalculateValueTextRect()
{
    if (!mDrawValue) {
        mValueTextRect = IRECT(); // Empty rect if not drawing value
        return;
    }

    mValueTextRect = IRECT(mRECT.R - mReservedTextWidth, mRECT.T, mRECT.R, mRECT.B)
        .GetVPadded(mValueTextPadding);
}

void ReacomaSlider::Draw(IGraphics& g)
{
  double value = GetValue();
  IRECT trackRect, filledTrackRect, handleRect;
  float currentHandleDim = mCurrentHandleThickness;

  IRECT sliderTrackArea = mRECT;
  if (mDrawValue) {
    sliderTrackArea.R -= mReservedTextWidth;
  }

  float trackY = sliderTrackArea.MH() - (mTrackThickness / 2.f);
  trackRect = IRECT(sliderTrackArea.L, trackY, sliderTrackArea.R, trackY + mTrackThickness);

  float filledWidth = static_cast<float>(value * sliderTrackArea.W());
  filledTrackRect = IRECT(sliderTrackArea.L, trackY, sliderTrackArea.L + filledWidth, trackY + mTrackThickness);

  float handleX = sliderTrackArea.L + filledWidth - (currentHandleDim / 2.f);
  handleX = std::max(sliderTrackArea.L, std::min(handleX, sliderTrackArea.R - currentHandleDim));
  handleRect = IRECT(handleX, mRECT.T, handleX + currentHandleDim, mRECT.B);

  g.FillRect(mTrackBackgroundColor, trackRect);
  g.FillRect(mTrackFillColor, filledTrackRect);
  g.FillRect(mCurrentSliderHandleColor, handleRect);

  if (mDrawValue && GetParam()) {
    WDL_String displayStr;
    GetParam()->GetDisplay(displayStr);

    IText textStyle = mValueTextStyle;
    textStyle.mAlign = EAlign::Center;
    textStyle.mVAlign = EVAlign::Middle;

    g.DrawText(textStyle, displayStr.Get(), mValueTextRect);
  }

  // Add this line to draw the border
  g.DrawRect(COLOR_BLACK, mRECT); // You can change COLOR_BLACK to any color you prefer

  if (IsDisabled()) {
    g.FillRect(COLOR_GRAY.WithOpacity(0.5f), mRECT, &mBlend);
  }
}

void ReacomaSlider::OnMouseDown(float x, float y, const IMouseMod& mod)
{
    if (mod.L && mDrawValue && GetParam() && GetUI() && mValueTextRect.W() > 0 && mValueTextRect.H() > 0 && mValueTextRect.Contains(x, y)) {
        WDL_String str;
        GetParam()->GetDisplay(str);

        IText textEntryStyle = mValueTextStyle;

        GetUI()->CreateTextEntry(*this, textEntryStyle, mValueTextRect, str.Get());
        return;
    }

    mIsDragging = true;
    if (GetAnimationFunction()) { // Stop any hover/press animation when drag begins
        SetAnimation(nullptr);
    }
    AnimateState(true, true); // Set "dragging" visual state
    UpdateValueFromPos(x, y);
}

void ReacomaSlider::OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod)
{
    if (mIsDragging) {
        if (GetAnimationFunction()) {
             SetAnimation(nullptr);
             // Ensure visual state is correctly set for dragging if animation was interrupted
             mCurrentHandleThickness = mDragHandleThickness;
             mCurrentSliderHandleColor = mDragHandleColor;
        }
        UpdateValueFromPos(x, y); // This calls SetValue which calls SetDirty(true)
        SetDirty(true);          // Explicitly ask for redraw for this drag step
    }
}

void ReacomaSlider::UpdateValueFromPos(float x, float y)
{
    float val = 0.f;
    IRECT interactiveArea = mRECT; // Start with full rect

    if (mDrawValue) {
        interactiveArea.R -= mReservedTextWidth;
    }
    
    if (interactiveArea.W() <= 0.f) return;
    float clampedX = std::max(interactiveArea.L, std::min(x, interactiveArea.R));
    val = (clampedX - interactiveArea.L) / interactiveArea.W();
    
    val = Clip(val, 0.f, 1.f);
    SetValue(val);
}

void ReacomaSlider::AnimateState(bool isOver, bool isDragging)
{
    float targetThickness = mBaseHandleThickness;
    IColor targetColor = mSliderHandleColor;
    EAnimationState newAnimationTargetState = EAnimationState::kNone;

    if (isDragging) {
        targetThickness = mDragHandleThickness;
        targetColor = mDragHandleColor;
        newAnimationTargetState = EAnimationState::kDrag;
    } else if (isOver) {
        targetThickness = mHoverHandleThickness;
        targetColor = mMouseOverHandleColor;
        newAnimationTargetState = EAnimationState::kHover;
    }
    bool visuallyAtTarget = (std::fabs(mCurrentHandleThickness - targetThickness) < 0.01f && mCurrentSliderHandleColor == targetColor);

    if (visuallyAtTarget && mAnimationTargetState == newAnimationTargetState && !GetAnimationFunction()) {
      return;
    }
    
    mAnimationTargetState = newAnimationTargetState; // Set the new target

    if(GetAnimationFunction()) SetAnimation(nullptr); // Stop existing animation

    const float startThickness = mCurrentHandleThickness;
    const IColor startColor = mCurrentSliderHandleColor;

    // If already at target (e.g., after drag animation ended but still dragging), don't animate, just ensure state
    if (startThickness == targetThickness && startColor.ToColorCode() == targetColor.ToColorCode()) {
        mCurrentHandleThickness = targetThickness;
        mCurrentSliderHandleColor = targetColor;
        SetDirty(false);
        return;
    }
    
    SetAnimation([this, startThickness, targetThickness, startColor, targetColor](IControl* pCaller) {
        float progress = static_cast<float>(GetAnimationProgress());
        mCurrentHandleThickness = Lerp(startThickness, targetThickness, progress);
        mCurrentSliderHandleColor = IColor::LinearInterpolateBetween(startColor, targetColor, progress);
        SetDirty(false);
        if (progress >= 1.f) {
            OnEndAnimation();
        }
    }, mAnimationDurationMs);
}

void ReacomaSlider::OnMouseOver(float x, float y, const IMouseMod& mod)
{
    IControl::OnMouseOver(x, y, mod);
    if (!mIsDragging)
        AnimateState(true, false); // Animate to hover state
}

void ReacomaSlider::OnMouseOut()
{
    IControl::OnMouseOut();
    if (!mIsDragging)
        AnimateState(false, false); // Animate to non-hover (base) state
}

void ReacomaSlider::OnMouseUp(float x, float y, const IMouseMod& mod)
{
    if (mIsDragging) {
        mIsDragging = false;
        // After dragging, determine if mouse is still over the control to transition to hover or base state
        AnimateState(mMouseIsOver, false);
    }
}

void ReacomaSlider::OnEndAnimation()
{
    // Snap to the final target values based on mAnimationTargetState
    // This ensures precision after animation.
    if (mAnimationTargetState == EAnimationState::kDrag) {
        mCurrentHandleThickness = mDragHandleThickness;
        mCurrentSliderHandleColor = mDragHandleColor;
    } else if (mAnimationTargetState == EAnimationState::kHover) {
        mCurrentHandleThickness = mHoverHandleThickness;
        mCurrentSliderHandleColor = mMouseOverHandleColor;
    } else { // kNone implies animation was to return to base state
        mCurrentHandleThickness = mBaseHandleThickness;
        mCurrentSliderHandleColor = mSliderHandleColor;
    }
    // Don't reset mAnimationTargetState here, AnimateState will set it
    IControl::OnEndAnimation(); // Important to call base to clear animation function
    SetDirty(false);
}

void ReacomaSlider::OnMouseWheel(float x, float y, const IMouseMod& mod, float d)
{
    if (!GetParam()) return;

    double oldValue = GetValue();
    double step = GetParam()->GetStep();
    double gearing = (step > 0.000001) ? step : 0.01; // Use param step if available, else default

    if (mod.C || mod.S) // Fine adjustment with Ctrl/Shift
        gearing *= 0.1;

    double newValue = oldValue + (static_cast<double>(d) * gearing);
    SetValue(Clip(newValue, 0.0, 1.0));
    SetDirty(true);
}

} // namespace igraphics
} // namespace iplug
