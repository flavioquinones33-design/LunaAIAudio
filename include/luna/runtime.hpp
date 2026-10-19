#pragma once
#include "engine.hpp"
#include <atomic>
#include <cstdint>
#include <memory>

namespace luna {
enum class RuntimeMode : std::uint8_t { Legacy = 0, Next = 1, Passthrough = 2 };
enum class EngineHealth : std::uint8_t { Healthy = 0, Degraded = 1, Failed = 2, Unavailable = 3 };
enum class FallbackReason : std::uint8_t { None = 0, BackendUnavailable = 1, InvalidOutput = 2 };

struct RuntimeSnapshot {
    RuntimeMode requested = RuntimeMode::Legacy;
    RuntimeMode active = RuntimeMode::Legacy;
    EngineHealth health = EngineHealth::Healthy;
    FallbackReason lastFallbackReason = FallbackReason::None;
    std::uint64_t fallbackCount = 0;
    std::uint64_t initializationFailures = 0;
    std::uint64_t processingFailures = 0;
};

const char* runtimeModeName(RuntimeMode mode) noexcept;
const char* engineHealthName(EngineHealth health) noexcept;
const char* fallbackReasonName(FallbackReason reason) noexcept;
RuntimeMode parseRuntimeMode(const char* value);
std::unique_ptr<NoiseSuppressionEngine> makePassthrough();

class RuntimeEngine final : public NoiseSuppressionEngine {
    std::unique_ptr<NoiseSuppressionEngine> next_;
    std::unique_ptr<NoiseSuppressionEngine> legacy_;
    std::unique_ptr<NoiseSuppressionEngine> passthrough_;
    NoiseSuppressionEngine* activeEngine_ = nullptr;
    RuntimeMode requested_ = RuntimeMode::Legacy;
    std::atomic<RuntimeMode> active_{RuntimeMode::Legacy};
    std::atomic<EngineHealth> health_{EngineHealth::Healthy};
    std::atomic<FallbackReason> lastReason_{FallbackReason::None};
    std::atomic<std::uint64_t> fallbackCount_{0};
    std::atomic<std::uint64_t> initializationFailures_{0};
    std::atomic<std::uint64_t> processingFailures_{0};

    static bool compatible(const NoiseSuppressionEngine* engine) noexcept;
    NoiseSuppressionEngine* engineFor(RuntimeMode mode) noexcept;
    RuntimeMode nextFallback(RuntimeMode mode) const noexcept;
    bool switchTo(RuntimeMode mode, FallbackReason reason, bool initialization) noexcept;
    bool outputValid(const float* output) const noexcept;
public:
    RuntimeEngine(RuntimeMode requested,
                  std::unique_ptr<NoiseSuppressionEngine> next,
                  std::unique_ptr<NoiseSuppressionEngine> legacy,
                  std::unique_ptr<NoiseSuppressionEngine> passthrough);
    explicit RuntimeEngine(RuntimeMode requested = RuntimeMode::Legacy);

    const char* name() const noexcept override { return "Luna modular runtime"; }
    unsigned sampleRate() const noexcept override { return sample_rate; }
    std::size_t frameSize() const noexcept override { return 480; }
    std::size_t latencySamples() const noexcept override { return 480; }
    void process(const float* input, float* output) noexcept override;
    RuntimeSnapshot snapshot() const noexcept;
};

std::unique_ptr<RuntimeEngine> makeRuntimeEngine(RuntimeMode requested = RuntimeMode::Legacy);
}
