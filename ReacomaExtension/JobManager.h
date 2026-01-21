#pragma once

#include <deque>
#include <list>
#include <memory>
#include <mutex>
#include <vector>

#include "Algorithms/IAlgorithm.h"
#include "Algorithms/ProcessingJob.h"

// Forward declarations
class MediaItem;
class ReaProject;

class JobManager {
public:
    JobManager();
    ~JobManager();

    void StartBatch(const std::vector<MediaItem *> &items,
                    IAlgorithm *algorithmPrototype, ProcessingMode mode);
    void Cancel();
    void Tick(); // Called from OnIdle

    bool IsProcessing() const;
    double GetOverallProgress() const;

private:
    void ManageProcessingJobs();
    void FinalizeJob(ProcessingJob *job);

    unsigned int mConcurrencyLimit = 1;
    std::deque<MediaItem *> mPendingItemsQueue;
    std::list<std::unique_ptr<ProcessingJob>> mActiveJobs;
    std::deque<std::unique_ptr<ProcessingJob>> mFinalizationQueue;

    size_t mTotalBatchItems = 0;
    double mLastReportedProgress = 0.0;

    ReaProject *mBatchUndoProject = nullptr;
    bool mIsProcessingBatch = false;
    bool mIsCancellationRequested = false;

    IAlgorithm *mCurrentPrototype = nullptr;
};
