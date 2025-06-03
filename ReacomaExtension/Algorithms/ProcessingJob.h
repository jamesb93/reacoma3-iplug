#pragma once

#include <memory>
#include "IAlgorithm.h"
#include "ReacomaExtension.h" // NEW: Include the full header to get the enum definition

// Forward declare these to avoid including their full headers here
class MediaItem;
class ReacomaExtension;

class ProcessingJob {
public:
    // The factory method can now correctly see the enum
    static std::unique_ptr<ProcessingJob> Create(ReacomaExtension::EAlgorithmChoice algoChoice, MediaItem* item, ReacomaExtension* provider);

    void Start();
    bool IsFinished();
    void Finalize();

    std::unique_ptr<IAlgorithm> mAlgorithm;
    MediaItem* mItem;

    // Make the constructor public so make_unique can access it.
    ProcessingJob(std::unique_ptr<IAlgorithm> algorithm, MediaItem* item);
};
