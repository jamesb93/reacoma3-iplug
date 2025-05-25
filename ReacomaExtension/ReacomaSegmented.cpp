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
                // Single segment keeps its original rounded rectangle drawing
                g.FillRoundRect(fillColor, segmentRect, mCornerRadius);
                g.DrawRoundRect(mBorderColor, segmentRect, mCornerRadius);
            } else if (i == 0) { // First segment (now rectangular)
                g.FillRect(fillColor, segmentRect); // Fill the entire segment
                
                // Draw Borders for the first segment (rectangular)
                // g.DrawArc(mBorderColor, segmentRect.L + mCornerRadius, segmentRect.MH(), mCornerRadius, 90.f, 270.f); // Left round border - REMOVED
                g.DrawLine(mBorderColor, segmentRect.L, segmentRect.T, segmentRect.R, segmentRect.T); // Top border (full width)
                g.DrawLine(mBorderColor, segmentRect.L, segmentRect.B, segmentRect.R, segmentRect.B); // Bottom border (full width)
                g.DrawLine(mBorderColor, segmentRect.L, segmentRect.T, segmentRect.L, segmentRect.B); // Left border
            } else if (i == mSegmentLabels.size() - 1) { // Last segment (now rectangular)
                g.FillRect(fillColor, segmentRect); // Fill the entire segment

                // Draw Borders for the last segment (rectangular)
                // g.DrawArc(mBorderColor, segmentRect.R - mCornerRadius, segmentRect.MH(), mCornerRadius, -90.f, 90.f); // Right round border - REMOVED
                g.DrawLine(mBorderColor, segmentRect.L, segmentRect.T, segmentRect.R, segmentRect.T); // Top border (full width)
                g.DrawLine(mBorderColor, segmentRect.L, segmentRect.B, segmentRect.R, segmentRect.B); // Bottom border (full width)
                g.DrawLine(mBorderColor, segmentRect.R, segmentRect.T, segmentRect.R, segmentRect.B); // Right border
            } else { // Middle segments (already rectangular)
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
    
