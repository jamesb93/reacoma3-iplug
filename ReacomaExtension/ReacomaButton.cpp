#include "ReacomaButton.h"
#include "IGraphics.h"
#include "IPlugUtilities.h"
#include "IGraphicsConstants.h"
#include "IGraphicsStructs.h"
#include <cmath>
#include <algorithm>

namespace iplug {
namespace igraphics {

ReacomaButton::ReacomaButton(const IRECT& bounds,
                             const char* label,
                             IActionFunction actionFunction,
                             const IColor&bgColor,
                             const IColor&fgColor,
                             const IColor&pressedColor,
                             const IColor&hoverColor,
                             const IText& textStyle,
                             float frameThickness,
                             float cornerRadius)
  : IButtonControlBase(bounds, actionFunction)
  , mLabel(label)
  , mTextStyle(textStyle)
  , mBackgroundColor(bgColor)
  , mForegroundColor(fgColor)
  , mPressedColor(pressedColor)
  , mHoverColor(hoverColor)
  , mCurrentBackgroundColor(bgColor)
  , mFrameThickness(frameThickness)
  , mCornerRadius(cornerRadius)
  , mIsPressed(false)
  , mAnimationDurationMs(80)
{
  mTextStyle.mFGColor = mForegroundColor;
  SetTooltip(label);
}

void ReacomaButton::SetColors(const IColor& bgColor, const IColor& fgColor, const IColor& pressedColor, const IColor& hoverColor)
{
  mBackgroundColor = bgColor;
  mForegroundColor = fgColor;
  mPressedColor = pressedColor;
  mHoverColor = hoverColor;
  mTextStyle.mFGColor = mForegroundColor;

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
  SetTooltip(label);
  SetDirty(false);
}

void ReacomaButton::Draw(IGraphics& g)
{
  IRECT areaToDraw = mRECT;
  if (mFrameThickness > 0.f) {
    areaToDraw.Pad(mFrameThickness / 2.f);
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

  if (mLabel.GetLength())
  {
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
      SetAnimation(nullptr);
  }

  const IColor startColor = mCurrentBackgroundColor;

  if (startColor.R != targetColor.R) {
    SetAnimation([this, startColor, targetColor](IControl* pCaller) {
      float progress = static_cast<float>(GetAnimationProgress());
      mCurrentBackgroundColor = IColor::LinearInterpolateBetween(startColor, targetColor, progress);
      SetDirty(false);
      if (progress >= 1.f) {
        OnEndAnimation();
      }
    }, mAnimationDurationMs);
  } else {
      mCurrentBackgroundColor = targetColor;
      SetDirty(false);
  }
}

void ReacomaButton::OnMouseOver(float x, float y, const IMouseMod& mod)
{
  IControl::OnMouseOver(x, y, mod);
  if (!mIsPressed)
    AnimateState(true, false);
}

void ReacomaButton::OnMouseOut()
{
  IControl::OnMouseOut();
  if (!mIsPressed)
    AnimateState(false, false);
}

void ReacomaButton::OnMouseDown(float x, float y, const IMouseMod& mod)
{
  mIsPressed = true;
  AnimateState(true, true);
  IButtonControlBase::OnMouseDown(x, y, mod);
}

void ReacomaButton::OnMouseUp(float x, float y, const IMouseMod& mod)
{
  mIsPressed = false;
  bool stillOver = false;
  if (GetUI()) {
//      FIXME
//    int ctrlIdx = GetUI()->GetMouseControlIdx(x, y, false);
//    stillOver = (ctrlIdx == GetUI()->GetControlIdx(this));
  }
  mMouseIsOver = stillOver;
  AnimateState(stillOver, false);
  IButtonControlBase::OnMouseUp(x, y, mod);
}

void ReacomaButton::OnEndAnimation()
{
  if (mIsPressed) {
      mCurrentBackgroundColor = mPressedColor;
  } else if (mMouseIsOver) {
      mCurrentBackgroundColor = mHoverColor;
  } else {
      mCurrentBackgroundColor = mBackgroundColor;
  }

  IControl::OnEndAnimation();
  SetDirty(false);
}

}
}
