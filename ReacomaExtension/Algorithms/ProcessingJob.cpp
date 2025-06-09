#include "ProcessingJob.h"
#include "NoveltySliceAlgorithm.h"
#include "HPSSAlgorithm.h"
#include "NMFAlgorithm.h"

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

std::unique_ptr<ProcessingJob>
ProcessingJob::Create(ReacomaExtension::EAlgorithmChoice algoChoice,
                      MediaItem *item, ReacomaExtension *provider) {
    std::unique_ptr<IAlgorithm> algorithm = nullptr;
    const IAlgorithm *prototypeAlgorithm = nullptr;

    switch (algoChoice) {
    case ReacomaExtension::kNoveltySlice:
        algorithm = std::make_unique<NoveltySliceAlgorithm>(provider);
        prototypeAlgorithm = provider->GetNoveltySliceAlgorithm();
        break;
    case ReacomaExtension::kHPSS:
        algorithm = std::make_unique<HPSSAlgorithm>(provider);
        prototypeAlgorithm = provider->GetHPSSAlgorithm();
        break;
    case ReacomaExtension::kNMF:
        algorithm = std::make_unique<NMFAlgorithm>(provider);
        prototypeAlgorithm = provider->GetNMFAlgorithm();
        break;
    }

    if (algorithm && prototypeAlgorithm) {
        algorithm->SetBaseParamIdx(prototypeAlgorithm->GetBaseParamIdx());
        return std::make_unique<ProcessingJob>(std::move(algorithm), item);
    }
    return nullptr;
}
