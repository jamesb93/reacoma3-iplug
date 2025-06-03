#pragma once

#include <memory>
#include "IAlgorithm.h"
#include "ReacomaExtension.h"

class MediaItem;
class ReacomaExtension;

class ProcessingJob {
public:
    static std::unique_ptr<ProcessingJob> Create(ReacomaExtension::EAlgorithmChoice algoChoice, MediaItem* item, ReacomaExtension* provider);

    void Start();
    bool IsFinished();
    void Finalize();
    
    double GetProgress() { return mAlgorithm->GetProgress(); }

    std::unique_ptr<IAlgorithm> mAlgorithm;
    MediaItem* mItem;

    ProcessingJob(std::unique_ptr<IAlgorithm> algorithm, MediaItem* item);
};
