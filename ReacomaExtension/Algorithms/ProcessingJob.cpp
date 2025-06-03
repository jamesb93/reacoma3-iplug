#include "ProcessingJob.h"
#include "NoveltySliceAlgorithm.h"
#include "HPSSAlgorithm.h"
#include "NMFAlgorithm.h"

ProcessingJob::ProcessingJob(std::unique_ptr<IAlgorithm> algorithm, MediaItem* item)
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

std::unique_ptr<ProcessingJob> ProcessingJob::Create(ReacomaExtension::EAlgorithmChoice algoChoice, MediaItem* item, ReacomaExtension* provider)
{
    std::unique_ptr<IAlgorithm> algorithm = nullptr;
    switch (algoChoice)
    {
        case ReacomaExtension::kNoveltySlice:
            algorithm = std::make_unique<NoveltySliceAlgorithm>(provider);
            break;
        case ReacomaExtension::kHPSS:
            algorithm = std::make_unique<HPSSAlgorithm>(provider);
            break;
        case ReacomaExtension::kNMF:
            algorithm = std::make_unique<NMFAlgorithm>(provider);
            break;
    }

    if (algorithm) {
        // Each new instance must have its parameters registered to access UI settings.
        algorithm->RegisterParameters();
        return std::make_unique<ProcessingJob>(std::move(algorithm), item);
    }
    return nullptr;
}
