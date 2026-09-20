#pragma once
#include <atomic>
#include <cstdint>
#include <cmath>

namespace luna {
double processCpuSeconds() noexcept;
struct MetricsSnapshot {
    float inputDb = -120, outputDb = -120, inputPeak = 0, outputPeak = 0;
    float processUs = 0, meanUs = 0, maxUs = 0, callbackUs = 0;
    std::uint64_t frames = 0, lateFrames = 0, lateCallbacks = 0, invalidSamples = 0;
};
class AudioMetrics {
    std::atomic<float> inputDb_{-120}, outputDb_{-120}, inputPeak_{0}, outputPeak_{0};
    std::atomic<float> last_{0}, mean_{0}, max_{0}, callback_{0};
    std::atomic<std::uint64_t> frames_{0}, late_{0}, lateCallbacks_{0}, invalid_{0};
    // Single audio-thread writer; UI reads only atomics. Reset by replacing pipeline while stopped.
    double totalUs_ = 0;
public:
    static_assert(std::atomic<float>::is_always_lock_free, "Requires lock-free float metrics");
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free, "Use an x64 build");
    static float db(double rms) noexcept { return rms > 0.000001 ? static_cast<float>(20 * std::log10(rms)) : -120.0f; }
    void frame(const float* in, const float* out, std::size_t n, float us) noexcept;
    void callback(float us, float budget) noexcept;
    void invalid() noexcept { invalid_.fetch_add(1, std::memory_order_relaxed); }
    MetricsSnapshot snapshot() const noexcept;
};
}
