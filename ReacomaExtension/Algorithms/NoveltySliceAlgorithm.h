// NoveltySliceAlgorithm.h
#pragma once
#include <memory> // For std::unique_ptr
#include <vector>

// Forward declare Reaper types and your main extension class
class MediaItem;
class ReacomaExtension; // Forward-declare ReacomaExtension

class NoveltySliceAlgorithm {
public:
    // Constructor now takes a ReacomaExtension pointer
    NoveltySliceAlgorithm(ReacomaExtension* apiProvider); // Changed
    ~NoveltySliceAlgorithm();

    bool ProcessItem(MediaItem* item);
    const char* GetName() const;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
