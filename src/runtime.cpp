#include "luna/runtime.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace luna {
namespace {
class PassthroughEngine final : public NoiseSuppressionEngine {
    std::array<float, 480> delay_{};
    std::size_t position_ = 0;
public:
    const char* name() const noexcept override { return "Luna passthrough"; }
    unsigned sampleRate() const noexcept override { return sample_rate; }
    std::size_t frameSize() const noexcept override { return 480; }
    std::size_t latencySamples() const noexcept override { return 480; }
    void process(const float* input, float* output) noexcept override {
        for (std::size_t i = 0; i < 480; ++i) {
            const float x = input && std::isfinite(input[i]) ? std::clamp(input[i], -1.0f, 1.0f) : 0.0f;
            output[i] = delay_[position_];
            delay_[position_] = x;
            position_ = (position_ + 1) % delay_.size();
        }
    }
};
}

const char* runtimeModeName(RuntimeMode mode) noexcept {
    switch (mode) {
        case RuntimeMode::Legacy: return "legacy";
        case RuntimeMode::Next: return "next";
        case RuntimeMode::Passthrough: return "passthrough";
    }
    return "unknown";
}
const char* engineHealthName(EngineHealth health) noexcept {
    switch (health) {
        case EngineHealth::Healthy: return "healthy";
        case EngineHealth::Degraded: return "degraded";
        case EngineHealth::Failed: return "failed";
        case EngineHealth::Unavailable: return "unavailable";
    }
    return "unknown";
}
const char* fallbackReasonName(FallbackReason reason) noexcept {
    switch (reason) {
        case FallbackReason::None: return "none";
        case FallbackReason::BackendUnavailable: return "backend_unavailable";
        case FallbackReason::InvalidOutput: return "invalid_output";
    }
    return "unknown";
}
RuntimeMode parseRuntimeMode(const char* value) {
    if (!value) throw std::invalid_argument("Missing runtime mode");
    if (std::strcmp(value, "legacy") == 0) return RuntimeMode::Legacy;
    if (std::strcmp(value, "next") == 0) return RuntimeMode::Next;
    if (std::strcmp(value, "passthrough") == 0) return RuntimeMode::Passthrough;
    throw std::invalid_argument("Runtime mode must be legacy, next, or passthrough");
}
std::unique_ptr<NoiseSuppressionEngine> makePassthrough() { return std::make_unique<PassthroughEngine>(); }

bool RuntimeEngine::compatible(const NoiseSuppressionEngine* engine) noexcept {
    return engine && engine->sampleRate() == sample_rate && engine->frameSize() == 480 && engine->latencySamples() == 480;
}
NoiseSuppressionEngine* RuntimeEngine::engineFor(RuntimeMode mode) noexcept {
    switch (mode) {
        case RuntimeMode::Next: return next_.get();
        case RuntimeMode::Legacy: return legacy_.get();
        case RuntimeMode::Passthrough: return passthrough_.get();
    }
    return nullptr;
}
RuntimeMode RuntimeEngine::nextFallback(RuntimeMode mode) const noexcept {
    return mode == RuntimeMode::Next ? RuntimeMode::Legacy : RuntimeMode::Passthrough;
}
bool RuntimeEngine::switchTo(RuntimeMode mode, FallbackReason reason, bool initialization) noexcept {
    auto* candidate = engineFor(mode);
    if (!compatible(candidate)) {
        if (initialization) initializationFailures_.fetch_add(1, std::memory_order_relaxed);
        if (mode == RuntimeMode::Passthrough) {
            activeEngine_ = nullptr;
            active_.store(RuntimeMode::Passthrough, std::memory_order_relaxed);
            health_.store(EngineHealth::Failed, std::memory_order_relaxed);
            lastReason_.store(reason, std::memory_order_relaxed);
            return false;
        }
        fallbackCount_.fetch_add(1, std::memory_order_relaxed);
        lastReason_.store(FallbackReason::BackendUnavailable, std::memory_order_relaxed);
        health_.store(EngineHealth::Degraded, std::memory_order_relaxed);
        return switchTo(nextFallback(mode), FallbackReason::BackendUnavailable, initialization);
    }
    activeEngine_ = candidate;
    active_.store(mode, std::memory_order_relaxed);
    if (mode != requested_) health_.store(EngineHealth::Degraded, std::memory_order_relaxed);
    else health_.store(EngineHealth::Healthy, std::memory_order_relaxed);
    if (reason != FallbackReason::None) lastReason_.store(reason, std::memory_order_relaxed);
    return true;
}
bool RuntimeEngine::outputValid(const float* output) const noexcept {
    if (!output) return false;
    for (std::size_t i = 0; i < 480; ++i)
        if (!std::isfinite(output[i]) || std::abs(output[i]) > 1.000001f) return false;
    return true;
}
RuntimeEngine::RuntimeEngine(RuntimeMode requested,
                             std::unique_ptr<NoiseSuppressionEngine> next,
                             std::unique_ptr<NoiseSuppressionEngine> legacy,
                             std::unique_ptr<NoiseSuppressionEngine> passthrough)
    : next_(std::move(next)), legacy_(std::move(legacy)), passthrough_(std::move(passthrough)), requested_(requested) {
    if (!passthrough_) passthrough_ = makePassthrough();
    switchTo(requested_, FallbackReason::None, true);
}
RuntimeEngine::RuntimeEngine(RuntimeMode requested)
    : RuntimeEngine(requested, nullptr, makeRNNoise(), makePassthrough()) {}

void RuntimeEngine::process(const float* input, float* output) noexcept {
    if (!output) return;
    if (!activeEngine_) {
        std::fill(output, output + 480, 0.0f);
        return;
    }
    activeEngine_->process(input, output);
    if (outputValid(output)) return;

    processingFailures_.fetch_add(1, std::memory_order_relaxed);
    const auto current = active_.load(std::memory_order_relaxed);
    if (current != RuntimeMode::Passthrough) {
        fallbackCount_.fetch_add(1, std::memory_order_relaxed);
        switchTo(nextFallback(current), FallbackReason::InvalidOutput, false);
        if (activeEngine_) activeEngine_->process(input, output);
    }
    if (!outputValid(output)) {
        processingFailures_.fetch_add(1, std::memory_order_relaxed);
        if (active_.load(std::memory_order_relaxed) != RuntimeMode::Passthrough) {
            fallbackCount_.fetch_add(1, std::memory_order_relaxed);
            switchTo(RuntimeMode::Passthrough, FallbackReason::InvalidOutput, false);
            if (activeEngine_) activeEngine_->process(input, output);
        }
    }
    for (std::size_t i = 0; i < 480; ++i) {
        if (!std::isfinite(output[i])) output[i] = 0.0f;
        else output[i] = std::clamp(output[i], -1.0f, 1.0f);
    }
}
RuntimeSnapshot RuntimeEngine::snapshot() const noexcept {
    return {requested_, active_.load(std::memory_order_relaxed), health_.load(std::memory_order_relaxed),
            lastReason_.load(std::memory_order_relaxed), fallbackCount_.load(std::memory_order_relaxed),
            initializationFailures_.load(std::memory_order_relaxed), processingFailures_.load(std::memory_order_relaxed)};
}
std::unique_ptr<RuntimeEngine> makeRuntimeEngine(RuntimeMode requested) {
    return std::make_unique<RuntimeEngine>(requested);
}
}
