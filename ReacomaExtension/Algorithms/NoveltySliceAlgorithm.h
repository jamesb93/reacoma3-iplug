// NoveltySliceAlgorithm.h
#pragma once
#include <memory> // For std::unique_ptr

class MediaItem; // Assuming this is already forward-declarable or defined safely

class NoveltySliceAlgorithm {
public:
    NoveltySliceAlgorithm();
    ~NoveltySliceAlgorithm(); // Must be defined in .cpp

    bool ProcessItem(MediaItem* item);
    const char* GetName() const;
    // Add other public methods here. Try to avoid FluCoMa types in their signatures.

private:
    struct Impl; // Forward declaration
    std::unique_ptr<Impl> mImpl;
};
