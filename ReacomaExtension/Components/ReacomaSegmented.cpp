#include "ReacomaSegmented.h"

namespace iplug {
namespace igraphics {
ReacomaSegmented::ReacomaSegmented(
    const IRECT &bounds, int paramIdx,
    const std::vector<std::string> &segmentLabels, const ReacomaTheme &theme,
    int itemsPerRow)
    : IControl(bounds, paramIdx), mSegmentLabels(segmentLabels), mTheme(theme),
      mItemsPerRow(itemsPerRow) {
    CalculateSegmentRects();
}

void ReacomaSegmented::CalculateSegmentRects() {
    mSegmentRects.clear();
    if (mSegmentLabels.empty())
        return;

    int numItems = static_cast<int>(mSegmentLabels.size());
    int nCols = mItemsPerRow > 0 ? mItemsPerRow : numItems;
    int nRows = (numItems + nCols - 1) / nCols;

    for (int i = 0; i < numItems; ++i) {
        int row = i / nCols;
        int col = i % nCols;

        IRECT rowRect = mRECT.SubRectVertical(nRows, row);
        mSegmentRects.push_back(rowRect.SubRectHorizontal(nCols, col));
    }
}

void ReacomaSegmented::Draw(IGraphics &g) {
    if (mSegmentLabels.empty())
        return;

    int currentIdx = GetParam() ? GetParam()->Int() : 0;
    const bool isDisabled = IsDisabled();

    // Define colors based on the control's state from the theme
    IColor activeColor = isDisabled ? mTheme.disabledBg : mTheme.active;
    IColor inactiveColor = isDisabled ? mTheme.disabledBg : mTheme.inactive;
    IColor hoverColor = isDisabled
                            ? inactiveColor
                            : mTheme.hover; // Don't show hover when disabled
    IColor textColor = isDisabled ? mTheme.disabled : mTheme.fg;

    IText currentTextStyle = mTheme.buttonStyle;
    currentTextStyle.mFGColor = textColor;

    // 1. Draw each segment's background fill individually
    for (size_t i = 0; i < mSegmentLabels.size(); ++i) {
        IColor segmentFillColor;
        if (i == currentIdx) {
            segmentFillColor = activeColor;
        } else if (i == mHoveredSegment && !isDisabled) {
            segmentFillColor = hoverColor;
        } else {
            segmentFillColor = inactiveColor;
        }
        g.FillRect(segmentFillColor, mSegmentRects[i]);
    }

    // 2. Draw the main border and dividers over everything for a clean look
    g.DrawRoundRect(mTheme.border, mRECT, mTheme.cornerRadius);

    if (mItemsPerRow <= 0 ||
        mSegmentLabels.size() <= static_cast<size_t>(mItemsPerRow)) {
        // Single row - use vertical dividers
        for (size_t i = 0; i < mSegmentLabels.size() - 1; ++i) {
            const IRECT &segmentRect = mSegmentRects[i];
            g.DrawLine(mTheme.border, segmentRect.R, segmentRect.T,
                       segmentRect.R, segmentRect.B);
        }
    } else {
        // Multi-row - draw borders for each segment to form a grid
        for (size_t i = 0; i < mSegmentLabels.size(); ++i) {
            g.DrawRect(mTheme.border, mSegmentRects[i]);
        }
    }

    // 3. Draw the text labels on top
    for (size_t i = 0; i < mSegmentLabels.size(); ++i) {
        g.DrawText(currentTextStyle, mSegmentLabels[i].c_str(),
                   mSegmentRects[i]);
    }
}

void ReacomaSegmented::OnMouseDown(float x, float y, const IMouseMod &mod) {
    if (IsDisabled())
        return;

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

void ReacomaSegmented::OnMouseOver(float x, float y, const IMouseMod &mod) {
    IControl::OnMouseOver(x, y, mod);
    int segment = GetSegmentForPos(x, y);
    if (segment != mHoveredSegment) {
        mHoveredSegment = segment;
        SetDirty(false);
    }
}

void ReacomaSegmented::OnMouseOut() {
    IControl::OnMouseOut();
    if (mHoveredSegment != -1) {
        mHoveredSegment = -1;
        SetDirty(false);
    }
}

int ReacomaSegmented::GetSegmentForPos(float x, float y) {
    if (mSegmentLabels.empty() || !mRECT.Contains(x, y))
        return -1;

    for (int i = 0; i < static_cast<int>(mSegmentRects.size()); ++i) {
        if (mSegmentRects[i].Contains(x, y))
            return i;
    }
    return -1;
}
} // namespace igraphics
} // namespace iplug