// ReacomaSlider.h
#pragma once

#include "IControl.h"
#include "IGraphicsStructs.h"
#include "IGraphicsConstants.h"
#include "IGraphics.h"
#include "IPlugUtilities.h" // For Lerp

namespace iplug {
namespace igraphics {

class ReacomaSlider : public IControl
{
public:
    ReacomaSlider(const IRECT& bounds, int paramIdx);

    void Draw(IGraphics& g) override;
    void OnMouseDown(float x, float y, const IMouseMod& mod) override;
    void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override;
    void OnMouseUp(float x, float y, const IMouseMod& mod) override;
    void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override;
    void OnMouseOver(float x, float y, const IMouseMod& mod) override;
    void OnMouseOut() override;
    void OnEndAnimation() override;

    void SetSwissColors(const IColor& trackBgColor, const IColor& trackFillColor, const IColor& handleColor, const IColor& hoverColor, const IColor& dragColor);
    void SetHandleThickness(float thickness);
    void SetTrackThickness(float thickness) { mTrackThickness = thickness; SetDirty(false); }
    
    void SetHorizontal(bool horizontal) {
        mIsHorizontal = horizontal;
        CalculateValueTextRect(); // Recalculate on orientation change
        SetDirty(false);
    }
    void SetAnimationDuration(int durationMs) { mAnimationDurationMs = durationMs; }

    void SetValueTextStyle(const IText& style) { mValueTextStyle = style; SetDirty(false); }
    void SetTrackFillColor(const IColor& color); // Declaration
    
    void SetDrawValue(bool draw) {
        mDrawValue = draw;
        CalculateValueTextRect(); // Recalculate as text rect might become active/inactive
        SetDirty(false);
    }

    // IControl Overrides for layout changes
    void OnRescale() override { IControl::OnRescale(); CalculateValueTextRect(); SetDirty(false); }


private:
    void UpdateValueFromPos(float x, float y);
    void AnimateState(bool isOver, bool isDragging);
    void CalculateValueTextRect(); // Helper to calculate mValueTextRect

    // Swiss Style Colors
    IColor mTrackBackgroundColor;
    IColor mTrackFillColor;
    IColor mSliderHandleColor;
    IColor mMouseOverHandleColor;
    IColor mDragHandleColor;

    float mBaseHandleThickness;
    float mTrackThickness;
    float mHoverHandleThickness;
    float mDragHandleThickness;
    float mCurrentHandleThickness;
    IColor mCurrentSliderHandleColor;

    bool mIsHorizontal;
    bool mIsDragging;
    int mAnimationDurationMs;
    enum class EAnimationState { kNone, kHover, kDrag, kUnHover, kUnDrag };
    EAnimationState mAnimationTargetState;

    // Members for value display
    IText mValueTextStyle;
    bool mDrawValue;
    float mValueTextPadding;
    float mReservedTextWidth;
    float mReservedTextHeight;
    IRECT mValueTextRect; // Stores the bounds of the value text display
};

} // namespace igraphics
} // namespace iplug
