#pragma once
#include "processor.hpp"
#include <filesystem>
#include <cstdio>

namespace luna {
class WavWriter {
    std::FILE* file_ = nullptr;
    std::filesystem::path path_;
    std::uint64_t samples_ = 0;
    bool finished_ = false;
public:
    explicit WavWriter(const std::filesystem::path&);
    ~WavWriter();
    WavWriter(const WavWriter&) = delete;
    WavWriter& operator=(const WavWriter&) = delete;
    void write(const float*, std::size_t);
    void finish();
};
struct WavResult {
    std::uint64_t samples = 0;
    double wallSeconds = 0, cpuSeconds = 0, inputRms = 0, outputRms = 0;
    MetricsSnapshot metrics;
};
WavResult processWav(const std::filesystem::path& input, const std::filesystem::path& output,
                     float strength = 1, bool enabled = true);
void generateFixture(const std::filesystem::path& output, double seconds);
WavResult benchmark(double seconds);
}
