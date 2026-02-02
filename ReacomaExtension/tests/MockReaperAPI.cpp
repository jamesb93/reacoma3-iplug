#include "MockReaperObjects.h"
#include "wdltypes.h"
#include <cstring>

#define REAPERAPI_IMPLEMENT
#include "reaper_plugin_functions.h"

std::vector<MockMarker> gLastMarkers;
std::vector<std::pair<double, double>> gLastRegions;

void InitMockReaperAPI() {
    GetActiveTake = [](MediaItem *item) -> MediaItem_Take * {
        return (MediaItem_Take *)item;
    };

    GetMediaItemTake_Source = [](MediaItem_Take *take) -> PCM_source * {
        return (PCM_source *)take;
    };

    GetMediaSourceSampleRate = [](PCM_source *src) -> int {
        return src ? (int)src->GetSampleRate() : 44100;
    };

    GetMediaSourceNumChannels = [](PCM_source *src) -> int {
        return src ? src->GetNumChannels() : 1;
    };

    GetMediaItemInfo_Value = [](MediaItem *item, const char *parm) -> double {
        if (std::strcmp(parm, "D_LENGTH") == 0) {
            if (item)
                return ((PCM_source *)item)->GetLength();
        }
        if (std::strcmp(parm, "D_POSITION") == 0)
            return 0.0;
        return 0.0;
    };

    GetMediaItemTakeInfo_Value = [](MediaItem_Take *take,
                                    const char *parm) -> double {
        if (std::strcmp(parm, "D_PLAYRATE") == 0)
            return 1.0;
        if (std::strcmp(parm, "D_STARTOFFS") == 0)
            return 0.0;
        return 0.0;
    };

    SetMediaItemInfo_Value = [](MediaItem *item, const char *parm,
                                double val) -> bool { return true; };

    GetNumTakeMarkers = [](MediaItem_Take *take) -> int {
        return (int)gLastMarkers.size();
    };

    DeleteTakeMarker = [](MediaItem_Take *take, int idx) -> bool {
        if (idx >= 0 && idx < (int)gLastMarkers.size()) {
            gLastMarkers.erase(gLastMarkers.begin() + idx);
        }
        return true;
    };

    SetTakeMarker = [](MediaItem_Take *take, int idx, const char *name,
                       double *pos, int *color) -> int {
        MockMarker m;
        if (pos)
            m.pos = *pos;
        if (name)
            m.name = name;
        if (color)
            m.color = *color;
        else
            m.color = 0;
        gLastMarkers.push_back(m);
        return (int)gLastMarkers.size() - 1;
    };

    AddProjectMarker2 = [](ReaProject *proj, bool isrgn, double pos,
                           double rgnend, const char *name, int index,
                           int color) -> int {
        if (isrgn) {
            gLastRegions.push_back({pos, rgnend});
        }
        return 0;
    };

    UpdateArrange = []() {};
    UpdateTimeline = []() {};

    GetItemProjectContext = [](MediaItem *item) -> ReaProject * {
        return (ReaProject *)0x1234;
    };

    ColorToNative = [](int r, int g, int b) -> int {
        return (r << 16) | (g << 8) | b;
    };

    PCM_Sink_CreateEx = [](ReaProject *proj, const char *filename,
                           const char *cfg, int cfg_sz, int nch, int srate,
                           bool buildpeaks) -> PCM_sink * { return nullptr; };

    PCM_Source_CreateFromFile = [](const char *filename) -> PCM_source * {
        return nullptr;
    };

    AddTakeToMediaItem = [](MediaItem *item) -> MediaItem_Take * {
        return (MediaItem_Take *)item;
    };

    GetMediaSourceFileName = [](PCM_source *src, char *filename,
                                int filename_sz) {
        if (filename)
            std::snprintf(filename, filename_sz, "mock_file.wav");
    };

    GetMediaSourceParent = [](PCM_source *src) -> PCM_source * {
        return (PCM_source *)nullptr;
    };

    GetSetMediaItemTakeInfo = [](MediaItem_Take *take, const char *parm,
                                 void *val) -> void * { return nullptr; };
}
