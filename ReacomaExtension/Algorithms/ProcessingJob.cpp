#include "ProcessingJob.h"
#include <memory>

ProcessingJob::ProcessingJob(std::unique_ptr<IAlgorithm> algorithm,
                             MediaItem *item)
    : mAlgorithm(std::move(algorithm)), mItem(item) {}

void ProcessingJob::Start() {
    if (mAlgorithm && mItem) {
        mAlgorithm->StartProcessItemAsync(mItem);
    }
}

bool ProcessingJob::IsFinished() {
    return mAlgorithm ? mAlgorithm->IsFinished() : true;
}

void ProcessingJob::Finalize() {
    if (mAlgorithm && mItem) {
        mAlgorithm->FinalizeProcess(mItem);
    }
}

void ProcessingJob::Cancel() {
    if (mAlgorithm) {
        mAlgorithm->Cancel();
    }
}

std::unique_ptr<ProcessingJob> ProcessingJob::Create(IAlgorithm *prototype,
                                                     MediaItem *item) {
    if (prototype) {
        std::unique_ptr<IAlgorithm> algorithm = prototype->CreateNew();
        if (algorithm) {
            algorithm->SetBaseParamIdx(prototype->GetBaseParamIdx());
            return std::make_unique<ProcessingJob>(std::move(algorithm), item);
        }
    }
    return nullptr;
}
