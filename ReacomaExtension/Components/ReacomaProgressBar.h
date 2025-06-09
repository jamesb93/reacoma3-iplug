#pragma once

#include "IControl.h"
#include "IGraphics.h"
#include "IGraphicsStructs.h"

#include <iomanip>
#include <sstream>
#include <string>

class ReacomaExtension;

using namespace iplug::igraphics;

class ReacomaProgressBar : public IControl {
  public:
    ReacomaProgressBar(const IRECT &bounds, const char *label = "")
        : IControl(bounds), mProgress(0.0), mLabel(label) {}

    void Draw(IGraphics &g) override {
        g.FillRect(mTrackColor, mRECT);

        IRECT progressRect = mRECT.GetFromLeft(mRECT.W() * mProgress);
        g.FillRect(mFillColor, progressRect);

        g.DrawRect(mFrameColor, mRECT);

        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << (mProgress * 100.0) << "%";
        std::string progressStr = ss.str();

        g.DrawText(mTextStyle, progressStr.c_str(), mRECT);

        if (IsDisabled()) {
            g.FillRect(mDisabledColor, mRECT);
        }
    }

    void SetProgress(double progress) {
        mProgress = std::max(0.0, std::min(progress, 1.0));
        SetDirty(false);
    }

    void SetColors(const IColor &track, const IColor &fill, const IColor &frame,
                   const IColor &text) {
        mTrackColor = track;
        mFillColor = fill;
        mFrameColor = frame;
        mTextStyle.mFGColor = text;
        SetDirty(false);
    }

  private:
    double mProgress;
    WDL_String mLabel;

    IText mTextStyle =
        IText(14.f, COLOR_WHITE, "ibmplex", EAlign::Center, EVAlign::Middle);

    IColor mTrackColor = IColor(255, 80, 80, 80);
    IColor mFillColor = IColor(255, 120, 120, 120);
    IColor mFrameColor = COLOR_BLACK;
    IColor mDisabledColor = COLOR_TRANSLUCENT;
};
