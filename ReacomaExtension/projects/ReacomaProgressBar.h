#pragma once

#include "IControl.h"
#include "IGraphics.h" // For IRECT, IColor, etc.
#include "IGraphicsStructs.h" // For IVStyle and IText

#include <string>
#include <sstream>
#include <iomanip> // For std::setprecision

// Forward declare to avoid including full ReacomaExtension.h if not needed elsewhere
class ReacomaExtension;

class ReacomaProgressBar : public IControl
{
public:
    ReacomaProgressBar(const IRECT& bounds, const char* label = "")
        : IControl(bounds), mProgress(0.0), mLabel(label)
    {
        // Correctly access and set members of the inherited IVStyle mStyle
//        mStyle.showLabel = true;
//        mStyle.labelPos = IVStyle::ELabelPos::kAbove; // Use IVStyle's enum
//        mStyle.textStyle.mSize = 12.f; // Example: Set the label text size
        // mStyle.textStyle.mFont = "ibmplex"; // Font for label is often set globally or by host
        // mStyle.textStyle.mFGColor = COLOR_BLACK; // Label color
        
        // mTextStyle is a separate member for drawing the percentage INSIDE the bar
        // mTextStyle initialization remains the same as it's specific to this control.
    }

    void Draw(IGraphics& g) override
    {
        // Draw background
        g.FillRect(mTrackColor, mRECT);

        // Draw filled progress rect
        IRECT progressRect = mRECT.GetFromLeft(mRECT.W() * mProgress);
        g.FillRect(mFillColor, progressRect);
        
        // Draw border
        g.DrawRect(mFrameColor, mRECT);

        // Draw percentage text
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << (mProgress * 100.0) << "%";
        std::string progressStr = ss.str();
        // This uses the mTextStyle member of ReacomaProgressBar for the text inside the bar
        g.DrawText(mTextStyle, progressStr.c_str(), mRECT);

        if (IsDisabled())
        {
            g.FillRect(mDisabledColor, mRECT);
        }
        // The actual label (mLabel) will be drawn by IControl::DrawLabel if mStyle.showLabel is true
    }

    void SetProgress(double progress)
    {
        mProgress = std::max(0.0, std::min(progress, 1.0));
        SetDirty(false);
    }

    // Optional: Methods to customize colors if needed
    void SetColors(const IColor& track, const IColor& fill, const IColor& frame, const IColor& text)
    {
        mTrackColor = track;
        mFillColor = fill;
        mFrameColor = frame;
        mTextStyle.mFGColor = text;
        SetDirty(false);
    }

private:
    double mProgress;
    WDL_String mLabel; // This label will be drawn by IControl's logic using mStyle
    
    // Style for the percentage text drawn *inside* the progress bar
    IText mTextStyle = IText(14.f, COLOR_WHITE, "ibmplex", EAlign::Center, EVAlign::Middle);
    
    // Colors for the progress bar itself
    IColor mTrackColor = IColor(255, 80, 80, 80);
    IColor mFillColor = IColor(255, 120, 120, 120);
    IColor mFrameColor = COLOR_BLACK;
    IColor mDisabledColor = COLOR_TRANSLUCENT;
};
