// ReacomaSegmented.cpp
#include "ReacomaSegmented.h"

ReacomaSegmented::ReacomaSegmented(const IRECT& bounds, int paramIdx, const std::vector<std::string>& segmentLabels,
    const IText& textStyle, const IColor& activeColor, const IColor& inactiveColor)
    : IControl(bounds, paramIdx), mSegmentLabels(segmentLabels), mTextStyle(textStyle), mActiveColor(activeColor), mInactiveColor(inactiveColor)
    {
        CalculateSegmentRects();
        mTextStyle.mVAlign = EVAlign::Middle;
        mTextStyle.mAlign = EAlign::Center;
    }
    
    void ReacomaSegmented::CalculateSegmentRects()
    {
        mSegmentRects.clear();
        if (mSegmentLabels.empty()) return;
        float segmentWidth = mRECT.W() / mSegmentLabels.size();
        for (size_t i = 0; i < mSegmentLabels.size(); ++i)
        {
            mSegmentRects.push_back(mRECT.SubRectHorizontal(mSegmentLabels.size(), i));
        }
    }
    
    void ReacomaSegmented::Draw(IGraphics& g)
    {
        if (mSegmentLabels.empty()) return;
        int currentIdx = GetParam() ? GetParam()->Int() : 0;
        
        for (size_t i = 0; i < mSegmentLabels.size(); ++i)
        {
            IRECT segmentRect = mSegmentRects[i];
            IColor fillColor = (i == currentIdx) ? mActiveColor : mInactiveColor;
            
            if (mSegmentLabels.size() == 1) {
                g.FillRoundRect(fillColor, segmentRect, mCornerRadius);
                g.DrawRoundRect(mBorderColor, segmentRect, mCornerRadius);
            } else if (i == 0) { // First segment
                // Fill the main rectangle part, stopping before the left rounded cap begins
                if (segmentRect.L + mCornerRadius < segmentRect.R) { // Ensure there's a rectangle to draw
                    g.FillRect(fillColor, IRECT(segmentRect.L + mCornerRadius, segmentRect.T, segmentRect.R, segmentRect.B));
                }
                g.FillCircle(fillColor, segmentRect.L + mCornerRadius, segmentRect.MH(), mCornerRadius); // Left round part
                
                // Draw Borders for the first segment
                g.DrawArc(mBorderColor, segmentRect.L + mCornerRadius, segmentRect.MH(), mCornerRadius, 90.f, 270.f); // Left round border
                g.DrawLine(mBorderColor, segmentRect.L + mCornerRadius, segmentRect.T, segmentRect.R, segmentRect.T); // Top border
                g.DrawLine(mBorderColor, segmentRect.L + mCornerRadius, segmentRect.B, segmentRect.R, segmentRect.B); // Bottom border
            } else if (i == mSegmentLabels.size() - 1) { // Last segment
                // Fill the main rectangle part, starting after the right rounded cap begins
                if (segmentRect.L < segmentRect.R - mCornerRadius) { // Ensure there's a rectangle to draw
                    g.FillRect(fillColor, IRECT(segmentRect.L, segmentRect.T, segmentRect.R - mCornerRadius, segmentRect.B));
                }
                g.FillCircle(fillColor, segmentRect.R - mCornerRadius, segmentRect.MH(), mCornerRadius); // Right round part
                
                // Draw Borders for the last segment
                g.DrawArc(mBorderColor, segmentRect.R - mCornerRadius, segmentRect.MH(), mCornerRadius, -90.f, 90.f); // Right round border
                g.DrawLine(mBorderColor, segmentRect.L, segmentRect.T, segmentRect.R - mCornerRadius, segmentRect.T); // Top border
                g.DrawLine(mBorderColor, segmentRect.L, segmentRect.B, segmentRect.R - mCornerRadius, segmentRect.B); // Bottom border
            } else { // Middle segments
                g.FillRect(fillColor, segmentRect);
                // Draw top and bottom borders for middle segments
                g.DrawLine(mBorderColor, segmentRect.L, segmentRect.T, segmentRect.R, segmentRect.T); // Top border
                g.DrawLine(mBorderColor, segmentRect.L, segmentRect.B, segmentRect.R, segmentRect.B); // Bottom border
            }
            
            // Draw separator line (except for the last one)
            if (i < mSegmentLabels.size() - 1)
            {
                g.DrawLine(mBorderColor, segmentRect.R, segmentRect.T, segmentRect.R, segmentRect.B);
            }
            
            g.DrawText(mTextStyle, mSegmentLabels[i].c_str(), segmentRect);
        }
    }
    
    void ReacomaSegmented::OnMouseDown(float x, float y, const IMouseMod& mod)
    {
        int clickedSegment = GetSegmentForPos(x, y);
        if (clickedSegment != -1 && GetParam())
        {
            // Normalize: (double)clickedSegment / (double)(numSegments - 1)
            // numSegments is mSegmentLabels.size()
            // maxIndex is mSegmentLabels.size() - 1 which is GetParam()->GetMax()
            if (mSegmentLabels.size() > 1) {
                SetValue((double)clickedSegment / (double)(mSegmentLabels.size() - 1));
            } else { // Single segment, always normalized to 0
                SetValue(0.0);
            }
            SetDirty(true);
        }
    }
    
    int ReacomaSegmented::GetSegmentForPos(float x, float y)
    {
        if (mSegmentLabels.empty() || !mRECT.Contains(x,y)) return -1;
        float segmentWidth = mRECT.W() / mSegmentLabels.size();
        int segmentIdx = static_cast<int>((x - mRECT.L) / segmentWidth);
        return std::max(0, std::min(segmentIdx, (int)mSegmentLabels.size() - 1));
    }
    
