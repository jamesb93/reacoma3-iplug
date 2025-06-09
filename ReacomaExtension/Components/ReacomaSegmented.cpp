#include "ReacomaSegmented.h"

ReacomaSegmented::ReacomaSegmented(
    const IRECT &bounds, int paramIdx,
    const std::vector<std::string> &segmentLabels, const IText &textStyle,
    const IColor &activeColor, const IColor &inactiveColor)
    : IControl(bounds, paramIdx), mSegmentLabels(segmentLabels),
      mTextStyle(textStyle), mActiveColor(activeColor),
      mInactiveColor(inactiveColor) {
    CalculateSegmentRects();
    mTextStyle.mVAlign = EVAlign::Middle;
    mTextStyle.mAlign = EAlign::Center;
}

void ReacomaSegmented::CalculateSegmentRects() {
    mSegmentRects.clear();
    if (mSegmentLabels.empty())
        return;
    for (size_t i = 0; i < mSegmentLabels.size(); ++i) {
        mSegmentRects.push_back(
            mRECT.SubRectHorizontal(mSegmentLabels.size(), i));
    }
}

void ReacomaSegmented::Draw(IGraphics &g) {
    if (mSegmentLabels.empty())
        return;
    int currentIdx = GetParam() ? GetParam()->Int() : 0;

    for (size_t i = 0; i < mSegmentLabels.size(); ++i) {
        IRECT segmentRect = mSegmentRects[i];
        IColor fillColor = (i == currentIdx) ? mActiveColor : mInactiveColor;

        if (mSegmentLabels.size() == 1) {
            g.FillRoundRect(fillColor, segmentRect, mCornerRadius);
            g.DrawRoundRect(mBorderColor, segmentRect, mCornerRadius);
        } else if (i == 0) {
            g.FillRect(fillColor, segmentRect);
            g.DrawLine(mBorderColor, segmentRect.L, segmentRect.T,
                       segmentRect.R, segmentRect.T);
            g.DrawLine(mBorderColor, segmentRect.L, segmentRect.B,
                       segmentRect.R, segmentRect.B);
            g.DrawLine(mBorderColor, segmentRect.L, segmentRect.T,
                       segmentRect.L, segmentRect.B);
        } else if (i == mSegmentLabels.size() - 1) {
            g.FillRect(fillColor, segmentRect);
            g.DrawLine(mBorderColor, segmentRect.L, segmentRect.T,
                       segmentRect.R, segmentRect.T);
            g.DrawLine(mBorderColor, segmentRect.L, segmentRect.B,
                       segmentRect.R, segmentRect.B);
            g.DrawLine(mBorderColor, segmentRect.R, segmentRect.T,
                       segmentRect.R, segmentRect.B);
        } else {
            g.FillRect(fillColor, segmentRect);
            g.DrawLine(mBorderColor, segmentRect.L, segmentRect.T,
                       segmentRect.R, segmentRect.T);
            g.DrawLine(mBorderColor, segmentRect.L, segmentRect.B,
                       segmentRect.R, segmentRect.B);
        }

        if (i < mSegmentLabels.size() - 1) {
            g.DrawLine(mBorderColor, segmentRect.R, segmentRect.T,
                       segmentRect.R, segmentRect.B);
        }

        g.DrawText(mTextStyle, mSegmentLabels[i].c_str(), segmentRect);
    }
}

void ReacomaSegmented::OnMouseDown(float x, float y, const IMouseMod &mod) {
    int clickedSegment = GetSegmentForPos(x, y);
    if (clickedSegment != -1 && GetParam()) {
        if (mSegmentLabels.size() > 1) {
            SetValue((double)clickedSegment /
                     (double)(mSegmentLabels.size() - 1));
        } else {
            SetValue(0.0);
        }
        SetDirty(true);
        GetDelegate()->LayoutUI(GetUI());
    }
}

int ReacomaSegmented::GetSegmentForPos(float x, float y) {
    if (mSegmentLabels.empty() || !mRECT.Contains(x, y))
        return -1;
    float segmentWidth = mRECT.W() / mSegmentLabels.size();
    int segmentIdx = static_cast<int>((x - mRECT.L) / segmentWidth);
    return std::max(0, std::min(segmentIdx, (int)mSegmentLabels.size() - 1));
}
