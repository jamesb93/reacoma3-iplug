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
    enum class EAnimationState {
        kNone,
        kHover,
        kDrag
    };

    ReacomaSlider(const IRECT& bounds, int paramIdx);

    // --- Drawing ---
    void Draw(IGraphics& g) override;

    // --- Mouse Events ---
    void OnMouseDown(float x, float y, const IMouseMod& mod) override;
    void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override;
    void OnMouseUp(float x, float y, const IMouseMod& mod) override;
    void OnMouseOver(float x, float y, const IMouseMod& mod) override;
    void OnMouseOut() override;
    void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override;
    void OnEndAnimation() override;

    // --- Customization ---
    void SetSwissColors(const IColor& trackBgColor, const IColor& trackFillColor, const IColor& handleColor, const IColor& hoverColor, const IColor& dragColor);
    void SetHandleThickness(float thickness); // Sets base, hover, and drag thickness based on factors
    void SetTrackThickness(float thickness) { mTrackThickness = thickness; SetDirty(false); }
    void SetTrackFillColor(const IColor& color);
    void SetTrackBackgroundColor(const IColor& color) { mTrackBackgroundColor = color; SetDirty(false); }

    void SetBorder(const IColor& color, float thickness);
    void SetBorderColor(const IColor& color);
    void SetBorderThickness(float thickness);

    void SetHorizontal(bool horizontal) { mIsHorizontal = horizontal; CalculateValueTextRect(); SetDirty(false); }
    void SetDrawValue(bool drawValue) { mDrawValue = drawValue; CalculateValueTextRect(); SetDirty(false); }
    void SetValueTextPadding(float padding) { mValueTextPadding = padding; CalculateValueTextRect(); SetDirty(false); }
    void SetReservedTextWidth(float width) { mReservedTextWidth = width; CalculateValueTextRect(); SetDirty(false); }
    void SetReservedTextHeight(float height) { mReservedTextHeight = height; CalculateValueTextRect(); SetDirty(false); }
    void SetValueTextStyle(const IText& style) { mValueTextStyle = style; SetDirty(false); }
    void SetAnimationDuration(int durationMs) { mAnimationDurationMs = durationMs; }


private:
    void UpdateValueFromPos(float x, float y);
    void AnimateState(bool isOver, bool isDragging);
    void CalculateValueTextRect();

    // Colors
    IColor mTrackBackgroundColor;
    IColor mTrackFillColor;
    IColor mSliderHandleColor;
    IColor mMouseOverHandleColor;
    IColor mDragHandleColor;
    IColor mBorderColor; // << NEW

    // Thicknesses
    float mBaseHandleThickness;
    float mTrackThickness;
    float mHoverHandleThickness;
    float mDragHandleThickness;
    float mBorderThickness; // << NEW

    // Current state
    float mCurrentHandleThickness;
    IColor mCurrentSliderHandleColor;
    bool mIsHorizontal;
    bool mIsDragging;

    // Animation
    int mAnimationDurationMs;
    EAnimationState mAnimationTargetState;

    // Value display
    IText mValueTextStyle;
    IRECT mValueTextRect;
    bool mDrawValue;
    float mValueTextPadding;
    float mReservedTextWidth;  // Width reserved for text when horizontal
    float mReservedTextHeight; // Height reserved for text when vertical
};

} // namespace igraphics
} // namespace iplug
