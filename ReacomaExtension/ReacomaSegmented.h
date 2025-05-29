#pragma once
#include "IControl.h"
#include <vector>
#include <string>
using namespace iplug::igraphics;

class ReacomaSegmented : public IControl
{
    public:
        ReacomaSegmented(const IRECT& bounds, int paramIdx, const std::vector<std::string>& segmentLabels,
        const IText& textStyle = DEFAULT_TEXT, const IColor& activeColor = DEFAULT_PRCOLOR,
        const IColor& inactiveColor = DEFAULT_BGCOLOR);
        
        void Draw(IGraphics& g) override;
        void OnMouseDown(float x, float y, const IMouseMod& mod) override;
        
    private:
        std::vector<std::string> mSegmentLabels;
        std::vector<IRECT> mSegmentRects;
        IText mTextStyle;
        IColor mActiveColor;
        IColor mInactiveColor;
        IColor mBorderColor = COLOR_BLACK;
        float mCornerRadius = 5.f; // For rounded look
        
        void CalculateSegmentRects();
        int GetSegmentForPos(float x, float y);
};
    
