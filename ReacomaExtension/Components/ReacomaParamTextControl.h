#pragma once

#include "IControl.h"
#include "IPlugParameter.h"

namespace iplug {
namespace igraphics {

class ReacomaParamTextControl : public IControl {
  public:
    ReacomaParamTextControl(const IRECT &bounds, int paramIdx,
                            const IText &textStyle = DEFAULT_TEXT);

    void Draw(IGraphics &g) override;
    void OnMouseDown(float x, float y, const IMouseMod &mod) override;
    void OnMouseOver(float x, float y, const IMouseMod &mod) override;
    void OnMouseOut() override;

  private:
    IRECT mLabelBounds;
    IRECT mValueBounds;
    IText mTextStyle;
    bool mMouseOverValueBox = false;
};

} // namespace igraphics
} // namespace iplug
