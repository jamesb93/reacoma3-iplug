#pragma once

#include "IControl.h"
#include "IGraphicsStructs.h"
#include "IGraphicsConstants.h" // For COLOR_BLACK, COLOR_RED etc.
#include "IGraphics.h"
#include "IPlugUtilities.h"

namespace iplug {
namespace igraphics {

// Define default styles directly in the header.
// These are now global const objects within the iplug::igraphics namespace.
const IColor REACOMA_BUTTON_BG_COLOR_DEFAULT = COLOR_WHITE;
const IColor REACOMA_BUTTON_FG_COLOR_DEFAULT = COLOR_BLACK;
const IColor REACOMA_BUTTON_PRESSED_COLOR_DEFAULT = COLOR_RED;
const IColor REACOMA_BUTTON_HOVER_COLOR_DEFAULT = COLOR_LIGHT_GRAY;
const IText  REACOMA_BUTTON_TEXT_STYLE_DEFAULT = IText(14.f, REACOMA_BUTTON_FG_COLOR_DEFAULT, "IBMPlexSans", EAlign::Center, EVAlign::Middle);

class ReacomaButton : public IButtonControlBase
{
public:
  ReacomaButton(const IRECT& bounds,
                const char* label,
                IActionFunction actionFunction, // CORRECTED IDENTIFIER
                const IColor&bgColor = REACOMA_BUTTON_BG_COLOR_DEFAULT, // Now uses const objects from this header
                const IColor&fgColor = REACOMA_BUTTON_FG_COLOR_DEFAULT,
                const IColor&pressedColor = REACOMA_BUTTON_PRESSED_COLOR_DEFAULT,
                const IColor&hoverColor = REACOMA_BUTTON_HOVER_COLOR_DEFAULT,
                const IText& textStyle = REACOMA_BUTTON_TEXT_STYLE_DEFAULT,
                float frameThickness = 1.f,
                float cornerRadius = 0.f);

  void Draw(IGraphics& g) override;

  void OnMouseDown(float x, float y, const IMouseMod& mod) override;
  void OnMouseUp(float x, float y, const IMouseMod& mod) override;
  void OnMouseOver(float x, float y, const IMouseMod& mod) override;
  void OnMouseOut() override;
  void OnEndAnimation() override;

  void SetColors(const IColor& bgColor, const IColor& fgColor, const IColor& pressedColor, const IColor& hoverColor);
  void SetTextStyle(const IText& textStyle);
  void SetLabel(const char* label);
  void SetAnimationDuration(int durationMs) { mAnimationDurationMs = durationMs; }

private:
  void AnimateState(bool isOver, bool isPressed);

  WDL_String mLabel;
  IText mTextStyle;

  IColor mBackgroundColor;
  IColor mForegroundColor;
  IColor mPressedColor;
  IColor mHoverColor;

  IColor mCurrentBackgroundColor;

  float mFrameThickness;
  float mCornerRadius;

  bool mIsPressed;
  int  mAnimationDurationMs;
};

} // namespace igraphics
} // namespace iplug
