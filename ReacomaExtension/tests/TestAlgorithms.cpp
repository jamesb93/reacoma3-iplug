#define APPROVALS_CATCH2_V3
#include <ApprovalTests.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

#include "Algorithms/NoveltySliceAlgorithm.h"
#include "Algorithms/AmpSliceAlgorithm.h"
#include "Algorithms/AmpGateAlgorithm.h"
#include "Algorithms/OnsetSliceAlgorithm.h"
#include "Algorithms/TransientSliceAlgorithm.h"
#include "Algorithms/HPSSAlgorithm.h"
#include "Algorithms/NMFAlgorithm.h"
#include "Algorithms/TransientAlgorithm.h"

#include "MockReaperObjects.h"
#include "Algorithms/IParameterProvider.h"
#include "AudioFileLoader.h"

#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <map>
#include <filesystem>
#include <thread>
#include <chrono>

class MockParameterProvider : public IParameterProvider {
public:
    void SetParam(int idx, double val) { mParams[idx] = val; }

    double GetParameterValue(int globalIdx) const override {
        auto it = mParams.find(globalIdx);
        if (it != mParams.end())
            return it->second;
        return 0.0;
    }

    void SetDefaults(const IAlgorithm &algo) {
        auto descriptors = algo.GetParamDescriptors();
        for (int i = 0; i < (int)descriptors.size(); ++i) {
            SetParam(algo.GetGlobalParamIdx(i), descriptors[i].defaultVal);
        }
    }

private:
    std::map<int, double> mParams;
};

std::string MarkersToString(const std::vector<MockMarker> &markers) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(6);
    for (const auto &m : markers) {
        ss << m.pos << "\n";
    }
    return ss.str();
}

void RunAlgorithmTest(IAlgorithm *algo, const AudioData &audio) {
    gLastMarkers.clear();
    gLastRegions.clear();

    MockSource source(audio.channels,
                      static_cast<int>(audio.samples.size() / audio.channels),
                      audio.sampleRate);
    source.SetData(audio.samples);

    algo->SyncParameters();
    algo->StartProcessItemAsync((MediaItem *)&source);

    while (!algo->IsFinished()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    algo->FinalizeProcess((MediaItem *)&source);
}

// Global audio data to avoid reloading for every test
AudioData gTestData;
std::string gTestFilePath;

int main(int argc, char *argv[]) {
    auto reporter = ApprovalTests::Approvals::useAsDefaultReporter(
        std::make_shared<ApprovalTests::QuietReporter>());
    InitMockReaperAPI();

    // Find the test file relative to this source file
    std::filesystem::path currentFile = __FILE__;
    std::filesystem::path projectRoot = currentFile.parent_path().parent_path();
    gTestFilePath = (projectRoot / "tests" / "test_audio.wav").string();

    try {
        gTestData = LoadWav(gTestFilePath);
    } catch (const std::exception &e) {
        std::cerr << "Failed to load test audio: " << e.what() << " at "
                  << gTestFilePath << std::endl;
        return 1;
    }

    return Catch::Session().run(argc, argv);
}

TEST_CASE("NoveltySlice Baseline", "[algorithms][baseline]") {
    MockParameterProvider provider;
    NoveltySliceAlgorithm algo(&provider);
    provider.SetDefaults(algo);

    RunAlgorithmTest(&algo, gTestData);
    ApprovalTests::Approvals::verify(MarkersToString(gLastMarkers));
}

TEST_CASE("AmpSlice Baseline", "[algorithms][baseline]") {
    MockParameterProvider provider;
    AmpSliceAlgorithm algo(&provider);
    provider.SetDefaults(algo);

    RunAlgorithmTest(&algo, gTestData);
    ApprovalTests::Approvals::verify(MarkersToString(gLastMarkers));
}

TEST_CASE("OnsetSlice Baseline", "[algorithms][baseline]") {
    MockParameterProvider provider;
    OnsetSliceAlgorithm algo(&provider);
    provider.SetDefaults(algo);

    RunAlgorithmTest(&algo, gTestData);
    ApprovalTests::Approvals::verify(MarkersToString(gLastMarkers));
}

TEST_CASE("TransientSlice Baseline", "[algorithms][baseline]") {
    MockParameterProvider provider;
    TransientSliceAlgorithm algo(&provider);
    provider.SetDefaults(algo);

    RunAlgorithmTest(&algo, gTestData);
    ApprovalTests::Approvals::verify(MarkersToString(gLastMarkers));
}

TEST_CASE("AmpGate Baseline", "[algorithms][baseline]") {
    MockParameterProvider provider;
    AmpGateAlgorithm algo(&provider);
    provider.SetDefaults(algo);

    RunAlgorithmTest(&algo, gTestData);
    ApprovalTests::Approvals::verify(MarkersToString(gLastMarkers));
}

TEST_CASE("HPSS Baseline", "[algorithms][baseline]") {
    MockParameterProvider provider;
    HPSSAlgorithm algo(&provider);
    provider.SetDefaults(algo);

    RunAlgorithmTest(&algo, gTestData);
    SUCCEED("HPSS Ran");
}

TEST_CASE("NMF Baseline", "[algorithms][baseline]") {
    MockParameterProvider provider;
    NMFAlgorithm algo(&provider);
    provider.SetDefaults(algo);

    RunAlgorithmTest(&algo, gTestData);
    SUCCEED("NMF Ran");
}

TEST_CASE("Transient Baseline", "[algorithms][baseline]") {
    MockParameterProvider provider;
    TransientAlgorithm algo(&provider);
    provider.SetDefaults(algo);

    RunAlgorithmTest(&algo, gTestData);
    SUCCEED("Transient Ran");
}