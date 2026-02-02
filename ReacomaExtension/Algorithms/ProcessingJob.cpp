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
        prototype->SyncParameters();
        std::unique_ptr<IAlgorithm> algorithm = prototype->CreateNew();
        if (algorithm) {
            algorithm->SetBaseParamIdx(prototype->GetBaseParamIdx());
            algorithm->InitParamValues(prototype->GetNumAlgorithmParams());
            for (int i = 0; i < prototype->GetNumAlgorithmParams(); ++i) {
                algorithm->SetParamValue(i, prototype->GetParamValue(i));
            }
            algorithm->SetProcessingMode(prototype->GetProcessingMode());
            return std::make_unique<ProcessingJob>(std::move(algorithm), item);
        }
    }
    return nullptr;
}
