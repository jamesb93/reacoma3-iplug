#include "ReacomaParamTextControl.h"
#include <algorithm>

namespace iplug {
namespace igraphics {

ReacomaParamTextControl::ReacomaParamTextControl(const IRECT &bounds,
                                                 int paramIdx,
                                                 const IText &textStyle)
    : IControl(bounds, paramIdx), mTextStyle(textStyle) {

    const float valueBoxWidth = 65.f;
    const float interLabelPadding = 10.f;

    float clampedWidth = std::min(valueBoxWidth, bounds.W());
    mValueBounds = mRECT.GetFromLeft(clampedWidth);
    mLabelBounds =
        mRECT.GetFromRight(mRECT.W() - clampedWidth - interLabelPadding);
    mTextStyle.mAlign = EAlign::Center;
}

void ReacomaParamTextControl::Draw(IGraphics &g) {
    const IParam *pParam = GetParam();

    char paramName[128] = "ERROR";
    WDL_String paramDisplay;

    if (pParam) {
        strcpy(paramName, pParam->GetName());
        pParam->GetDisplay(paramDisplay);
    }
    IText labelStyle = mTextStyle;
    labelStyle.mAlign = EAlign::Near;

    g.FillRect(mMouseOverValueBox && !IsDisabled() ? COLOR_LIGHT_GRAY
                                                   : COLOR_WHITE,
               mValueBounds);
    g.DrawRect(COLOR_BLACK, mValueBounds);
    g.DrawText(labelStyle, paramName, mLabelBounds.GetPadded(-5.f));
    g.DrawText(mTextStyle, paramDisplay.Get(), mValueBounds.GetPadded(-5.f));

    if (IsDisabled()) {
        g.FillRect(COLOR_GRAY.WithOpacity(0.7f), mRECT, &BLEND_75);
    }
}

void ReacomaParamTextControl::OnMouseDown(float x, float y,
                                          const IMouseMod &mod) {
    if (mValueBounds.Contains(x, y) && !IsDisabled()) {
        WDL_String currentText;
        if (GetParam())
            GetParam()->GetDisplay(currentText);

        GetUI()->CreateTextEntry(*this, mTextStyle, mValueBounds,
                                 currentText.Get());
    }
}

void ReacomaParamTextControl::OnMouseOver(float x, float y,
                                          const IMouseMod &mod) {
    bool isOverValue = mValueBounds.Contains(x, y);
    if (isOverValue != mMouseOverValueBox) {
        mMouseOverValueBox = isOverValue;
        SetDirty(false);
    }

    IControl::OnMouseOver(x, y, mod);
}

void ReacomaParamTextControl::OnMouseOut() {
    if (mMouseOverValueBox) {
        mMouseOverValueBox = false;
        SetDirty(false);
    }

    IControl::OnMouseOut();
}

} // namespace igraphics
} // namespace iplug
