#include "ReacomaButton.h"    // Include your header file FIRST
#include "IGraphics.h"
#include "IPlugUtilities.h"   // For iplug::Lerp
#include "IGraphicsConstants.h"
#include "IGraphicsStructs.h" // For IColor::LinearInterpolateBetween
#include <cmath>              // For std::fabs
#include <algorithm>          // For std::max/min if needed elsewhere

namespace iplug {
namespace igraphics {

// The static const definitions like 'const IColor ReacomaButton::SWISS_BUTTON_BG_COLOR_DEFAULT = ...;'
// are REMOVED from here. They are now global consts in ReacomaButton.h

ReacomaButton::ReacomaButton(const IRECT& bounds,
                             const char* label,
                             IActionFunction actionFunction, // CORRECTED IDENTIFIER
                             const IColor&bgColor,           // Receives default from .h
                             const IColor&fgColor,            // Receives default from .h
                             const IColor&pressedColor,       // Receives default from .h
                             const IColor&hoverColor,         // Receives default from .h
                             const IText& textStyle,          // Receives default from .h
                             float frameThickness,
                             float cornerRadius)
  : IButtonControlBase(bounds, actionFunction) // Pass the corrected actionFunction
  , mLabel(label)
  , mTextStyle(textStyle)
  , mBackgroundColor(bgColor)
  , mForegroundColor(fgColor)
  , mPressedColor(pressedColor)
  , mHoverColor(hoverColor)
  , mCurrentBackgroundColor(bgColor) // Initialize with the (potentially default) bgColor
  , mFrameThickness(frameThickness)
  , mCornerRadius(cornerRadius)
  , mIsPressed(false)
  , mAnimationDurationMs(80)
{
  mTextStyle.mFGColor = mForegroundColor; // Ensure text style uses the final fgColor
  SetTooltip(label);
}

void ReacomaButton::SetColors(const IColor& bgColor, const IColor& fgColor, const IColor& pressedColor, const IColor& hoverColor)
{
  mBackgroundColor = bgColor;
  mForegroundColor = fgColor;
  mPressedColor = pressedColor;
  mHoverColor = hoverColor;
  mTextStyle.mFGColor = mForegroundColor; // Update text color too

  // Update current background based on non-animated state
  if (!mIsPressed && !mMouseIsOver) mCurrentBackgroundColor = mBackgroundColor;
  else if (mIsPressed) mCurrentBackgroundColor = mPressedColor;
  else if (mMouseIsOver) mCurrentBackgroundColor = mHoverColor;

  SetDirty(false);
}

void ReacomaButton::SetTextStyle(const IText& textStyle)
{
  mTextStyle = textStyle;
  mTextStyle.mFGColor = mForegroundColor;
  SetDirty(false);
}

void ReacomaButton::SetLabel(const char* label)
{
  mLabel.Set(label);
  SetTooltip(label); // Also update tooltip when label changes
  SetDirty(false);
}

void ReacomaButton::Draw(IGraphics& g)
{
  // Make frame padding dependent on whether it's drawn, to prevent double-padding issues
  IRECT areaToDraw = mRECT;
  if (mFrameThickness > 0.f) {
    areaToDraw.Pad(mFrameThickness / 2.f); // Pad inwards for frame
  }

  if (mCornerRadius > 0.f)
  {
    g.FillRoundRect(mCurrentBackgroundColor, areaToDraw, mCornerRadius, &mBlend);
    if (mFrameThickness > 0.f)
      g.DrawRoundRect(mForegroundColor, areaToDraw, mCornerRadius, &mBlend, mFrameThickness);
  }
  else
  {
    g.FillRect(mCurrentBackgroundColor, areaToDraw, &mBlend);
    if (mFrameThickness > 0.f)
      g.DrawRect(mForegroundColor, areaToDraw, &mBlend, mFrameThickness);
  }

  // Draw Label within the original mRECT, not the padded areaToDraw for frame
  if (mLabel.GetLength())
  {
    // Ensure text style has the current foreground color
    IText currentTextStyle = mTextStyle;
    currentTextStyle.mFGColor = mForegroundColor;
    g.DrawText(currentTextStyle, mLabel.Get(), mRECT, &mBlend);
  }

  if (IsDisabled())
  {
    g.FillRect(COLOR_GRAY.WithOpacity(0.7f), mRECT, &BLEND_75);
  }
}

void ReacomaButton::AnimateState(bool isOver, bool isPressed)
{
  IColor targetColor = mBackgroundColor;

  if (isPressed) {
    targetColor = mPressedColor;
  } else if (isOver) {
    targetColor = mHoverColor;
  }
  
  if (GetAnimationFunction() && mCurrentBackgroundColor == targetColor) {
      return;
  }
  
  if(GetAnimationFunction()) {
      SetAnimation(nullptr); // Stop existing animation
  }

  const IColor startColor = mCurrentBackgroundColor;

  if (startColor.R != targetColor.R) {
    SetAnimation([this, startColor, targetColor](IControl* pCaller) {
      float progress = static_cast<float>(GetAnimationProgress());
      mCurrentBackgroundColor = IColor::LinearInterpolateBetween(startColor, targetColor, progress);
      SetDirty(false);
      if (progress >= 1.f) {
        OnEndAnimation(); // This will call IControl::OnEndAnimation()
      }
    }, mAnimationDurationMs);
  } else {
      mCurrentBackgroundColor = targetColor;
      SetDirty(false); // Ensure redraw if already at target but visuals might be off
  }
}

void ReacomaButton::OnMouseOver(float x, float y, const IMouseMod& mod)
{
  IControl::OnMouseOver(x, y, mod); // Call base to set mMouseIsOver = true
  if (!mIsPressed)
    AnimateState(true, false);
}

void ReacomaButton::OnMouseOut()
{
  IControl::OnMouseOut(); // Call base to set mMouseIsOver = false
  if (!mIsPressed)
    AnimateState(false, false);
}

void ReacomaButton::OnMouseDown(float x, float y, const IMouseMod& mod)
{
  mIsPressed = true;
  AnimateState(true, true);
  IButtonControlBase::OnMouseDown(x, y, mod); // This handles the action function call and mValue animation
}

void ReacomaButton::OnMouseUp(float x, float y, const IMouseMod& mod)
{
  mIsPressed = false;
  // Determine if mouse is still over this control when button is released
  bool stillOver = false;
  if (GetUI()) {
//      FIXME
//    int ctrlIdx = GetUI()->GetMouseControlIdx(x, y, false); // false for not mouseOver check
//    stillOver = (ctrlIdx == GetUI()->GetControlIdx(this));
  }
  mMouseIsOver = stillOver; // Explicitly set our understanding of mouse over state
  AnimateState(stillOver, false);
  IButtonControlBase::OnMouseUp(x, y, mod);
}

void ReacomaButton::OnEndAnimation()
{
  // This is for our visual animation.
  // Snap to the final target color of the visual animation.
  if (mIsPressed) {
      mCurrentBackgroundColor = mPressedColor;
  } else if (mMouseIsOver) { // Check our updated mMouseIsOver state
      mCurrentBackgroundColor = mHoverColor;
  } else {
      mCurrentBackgroundColor = mBackgroundColor;
  }
  
  IControl::OnEndAnimation(); // Call the base IControl version to free our animation function.
                              // IButtonControlBase::OnEndAnimation (which resets mValue)
                              // will be called by its own animation mechanism if it was started.
  SetDirty(false);
}

} // namespace igraphics
} // namespace iplug
