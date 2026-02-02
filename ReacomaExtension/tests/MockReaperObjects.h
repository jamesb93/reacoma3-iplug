#pragma once
#include "reaper_plugin.h"
#include <vector>
#include <string>

// A simple mock for PCM_source that returns generated audio
class MockSource : public PCM_source {
public:
    MockSource(int channels, int frames, int sampleRate)
        : mChannels(channels), mFrames(frames), mSampleRate(sampleRate) {
        mData.resize(channels * frames, 0.0);
    }

    PCM_source *Duplicate() override { return new MockSource(*this); }
    const char *GetType() override { return "MOCK"; }
    double GetLength() override {
        return static_cast<double>(mFrames) / mSampleRate;
    }
    int GetNumChannels() override { return mChannels; }
    double GetSampleRate() override { return mSampleRate; }

    void GetSamples(PCM_source_transfer_t *transfer) override {
        if (!transfer || transfer->nch != mChannels)
            return;

        int startFrame =
            static_cast<int>(transfer->time_s * transfer->samplerate);
        for (int i = 0; i < transfer->length; ++i) {
            int frame = startFrame + i;
            for (int c = 0; c < mChannels; ++c) {
                if (frame >= 0 && frame < mFrames) {
                    transfer->samples[i * mChannels + c] =
                        mData[frame * mChannels + c];
                } else {
                    transfer->samples[i * mChannels + c] = 0.0;
                }
            }
        }
    }

    void SetData(const std::vector<double> &data) { mData = data; }

    // Implement virtuals from PCM_source
    bool IsAvailable() override { return true; }
    bool SetFileName(const char *newfn) override { return true; }
    int PropertiesWindow(HWND hwndParent) override { return 0; }
    void SaveState(ProjectStateContext *ctx) override {}
    int LoadState(const char *firstline, ProjectStateContext *ctx) override {
        return 0;
    }
    void Peaks_Clear(bool deleteFile) override {}
    int PeaksBuild_Begin() override { return 0; }
    int PeaksBuild_Run() override { return 0; }
    void PeaksBuild_Finish() override {}

    void GetPeakInfo(PCM_source_peaktransfer_t *t) override {}

    // These were reported as not virtual in some versions of SDK or causing
    // issues
    int GetExtended(const char *key, void *val) { return 0; }
    bool IsZeroLength() { return mFrames == 0; }
    int GetSourceType() { return 0; }
    void SetSourceType(int t) {}

    virtual ~MockSource() {}

private:
    int mChannels;
    int mFrames;
    int mSampleRate;
    std::vector<double> mData;
};

// We don't need real implementations for MediaItem/Take for these tests
// but we need to track what was called.
struct MockMarker {
    double pos;
    std::string name;
    int color;
};

extern std::vector<MockMarker> gLastMarkers;
extern std::vector<std::pair<double, double>> gLastRegions;

void InitMockReaperAPI();
