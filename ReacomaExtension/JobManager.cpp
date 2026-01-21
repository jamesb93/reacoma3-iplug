#include "JobManager.h"
#include "Algorithms/IAlgorithm.h"
#include "Algorithms/ProcessingJob.h"

#include "wdltypes.h"
#include "reaper_plugin_functions.h"

#include <thread>

// Extern declarations for global API pointers initialized in ReacomaExtension
extern bool (*SetMediaItemInfo_Value)(MediaItem *item, const char *parmname,
                                      double newvalue);
extern ReaProject *(*GetItemProjectContext)(MediaItem *item);
extern void (*Undo_BeginBlock2)(ReaProject *proj);
extern void (*Undo_EndBlock2)(ReaProject *proj, const char *descchange,
                              int extraflags);
extern void (*UpdateArrange)();
extern void (*UpdateTimeline)();

JobManager::JobManager() {}

JobManager::~JobManager() { Cancel(); }

void JobManager::StartBatch(const std::vector<MediaItem *> &items,
                            IAlgorithm *algorithmPrototype,
                            ProcessingMode mode) {
    if (items.empty() || !algorithmPrototype)
        return;

    mConcurrencyLimit = std::thread::hardware_concurrency();
    mPendingItemsQueue.clear();
    mPendingItemsQueue.insert(mPendingItemsQueue.end(), items.begin(),
                              items.end());

    mTotalBatchItems = mPendingItemsQueue.size();
    mCurrentPrototype = algorithmPrototype;
    mCurrentPrototype->SetProcessingMode(mode);

    mIsProcessingBatch = true;
    mIsCancellationRequested = false;
    mLastReportedProgress = 0.0;
    mActiveJobs.clear();
    mFinalizationQueue.clear();

    mBatchUndoProject = GetItemProjectContext(mPendingItemsQueue.front());
    Undo_BeginBlock2(mBatchUndoProject);
}

void JobManager::Cancel() {
    if (mIsProcessingBatch) {
        mIsCancellationRequested = true;
    }
}

bool JobManager::IsProcessing() const { return mIsProcessingBatch; }

double JobManager::GetOverallProgress() const { return mLastReportedProgress; }

void JobManager::Tick() { ManageProcessingJobs(); }

void JobManager::ManageProcessingJobs() {
    if (!mIsProcessingBatch) {
        return;
    }

    if (mIsCancellationRequested) {
        for (auto &job : mActiveJobs) {
            job->Cancel();
            SetMediaItemInfo_Value(job->mItem, "C_LOCK", false);
        }

        mPendingItemsQueue.clear();
        mActiveJobs.clear();
        mFinalizationQueue.clear();

        mIsProcessingBatch = false;
        mIsCancellationRequested = false;

        Undo_EndBlock2(mBatchUndoProject, "Reacoma: Batch Process Cancelled",
                       -1);
        mBatchUndoProject = nullptr;

        UpdateArrange();
        UpdateTimeline();
        return;
    }

    for (auto it = mActiveJobs.begin(); it != mActiveJobs.end();) {
        if ((*it)->IsFinished()) {
            mFinalizationQueue.push_back(std::move(*it));
            it = mActiveJobs.erase(it);
        } else {
            ++it;
        }
    }

    while (mActiveJobs.size() < mConcurrencyLimit &&
           !mPendingItemsQueue.empty()) {
        MediaItem *itemToProcess = mPendingItemsQueue.front();
        mPendingItemsQueue.pop_front();

        auto job = ProcessingJob::Create(mCurrentPrototype, itemToProcess);
        if (job) {
            job->Start();
            mActiveJobs.push_back(std::move(job));
        }
    }

    if (!mFinalizationQueue.empty()) {
        auto &finishedJob = mFinalizationQueue.front();
        finishedJob->Finalize();
        mFinalizationQueue.pop_front();
    }

    if (mTotalBatchItems > 0) {
        double totalProgressUnits = 0.0;

        for (const auto &job : mActiveJobs) {
            totalProgressUnits += job->GetProgress();
        }

        size_t completedJobs =
            mTotalBatchItems - mPendingItemsQueue.size() - mActiveJobs.size();
        totalProgressUnits += static_cast<double>(completedJobs);

        double overallProgress = totalProgressUnits / mTotalBatchItems;
        if (overallProgress > mLastReportedProgress) {
            mLastReportedProgress = overallProgress;
        }
    }

    if (mPendingItemsQueue.empty() && mActiveJobs.empty() &&
        mFinalizationQueue.empty()) {
        mIsProcessingBatch = false;
        Undo_EndBlock2(mBatchUndoProject, "Reacoma: Process Batch", -1);
        mBatchUndoProject = nullptr;

        UpdateArrange();
        UpdateTimeline();
    }
}
