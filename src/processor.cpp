#include "luna/processor.hpp"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <ctime>
#ifdef _WIN32
#include <windows.h>
#endif

namespace luna {
double processCpuSeconds() noexcept {
#ifdef _WIN32
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) return 0;
    ULARGE_INTEGER k{}, u{};
    k.LowPart=kernel.dwLowDateTime; k.HighPart=kernel.dwHighDateTime;
    u.LowPart=user.dwLowDateTime; u.HighPart=user.dwHighDateTime;
    return static_cast<double>(k.QuadPart+u.QuadPart)*1e-7;
#else
    return double(std::clock())/CLOCKS_PER_SEC;
#endif
}
void AudioMetrics::frame(const float* in, const float* out, std::size_t n, float us) noexcept {
    double a = 0, b = 0;
    float p = 0, q = 0;
    for (std::size_t i = 0; i < n; ++i) {
        a += double(in[i]) * in[i]; b += double(out[i]) * out[i];
        p = std::max(p, std::abs(in[i])); q = std::max(q, std::abs(out[i]));
    }
    inputDb_.store(db(std::sqrt(a/n)), std::memory_order_relaxed);
    outputDb_.store(db(std::sqrt(b/n)), std::memory_order_relaxed);
    inputPeak_.store(p, std::memory_order_relaxed); outputPeak_.store(q, std::memory_order_relaxed);
    auto count = frames_.fetch_add(1, std::memory_order_relaxed) + 1;
    totalUs_ += us;
    last_.store(us, std::memory_order_relaxed);
    mean_.store(static_cast<float>(totalUs_ / count), std::memory_order_relaxed);
    max_.store(std::max(us, max_.load(std::memory_order_relaxed)), std::memory_order_relaxed);
    if (us > n * (1000000.0 / sample_rate)) late_.fetch_add(1, std::memory_order_relaxed);
}
void AudioMetrics::callback(float us, float budget) noexcept {
    callback_.store(us, std::memory_order_relaxed);
    if (us > budget) lateCallbacks_.fetch_add(1, std::memory_order_relaxed);
}
MetricsSnapshot AudioMetrics::snapshot() const noexcept {
    return {inputDb_.load(), outputDb_.load(), inputPeak_.load(), outputPeak_.load(),
        last_.load(), mean_.load(), max_.load(), callback_.load(), frames_.load(),
        late_.load(), lateCallbacks_.load(), invalid_.load()};
}
AudioProcessor::AudioProcessor(std::unique_ptr<NoiseSuppressionEngine> engine) : engine_(std::move(engine)) {
    if (!engine_ || engine_->sampleRate() != sample_rate || engine_->frameSize() == 0 ||
        engine_->frameSize() > 48000 || engine_->latencySamples() > 48000)
        throw std::invalid_argument("Engine must expose 48 kHz PCM, a bounded frame and latency");
    auto n = engine_->frameSize();
    input_.resize(n); output_.resize(n); wet_.resize(n); dry_.resize(n);
    delay_.resize(engine_->latencySamples());
}
void AudioProcessor::setStrength(float v) noexcept {
    strength_.store(std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 1.0f, std::memory_order_relaxed);
}
void AudioProcessor::processFrame() noexcept {
    auto start = std::chrono::steady_clock::now();
    // Keep engine state warm even in bypass, so switching does not reset its history.
    engine_->process(input_.data(), wet_.data());
    float target = enabled_.load(std::memory_order_relaxed) ? strength_.load(std::memory_order_relaxed) : 0;
    if (metrics_.snapshot().frames == 0) mix_ = target;
    const auto n = input_.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (delay_.empty()) dry_[i] = input_[i];
        else {
            dry_[i] = delay_[delayPosition_];
            delay_[delayPosition_] = input_[i];
            delayPosition_ = (delayPosition_ + 1) % delay_.size();
        }
        float w = mix_ + (target - mix_) * static_cast<float>(i + 1) / static_cast<float>(n);
        output_[i] = std::clamp(dry_[i] * (1 - w) + wet_[i] * w, -1.0f, 1.0f);
    }
    mix_ = target;
    auto us = std::chrono::duration<float, std::micro>(std::chrono::steady_clock::now() - start).count();
    metrics_.frame(input_.data(), output_.data(), n, us);
}
void AudioProcessor::process(const float* in, float* out, std::size_t count) noexcept {
    // One fixed frame of adapter delay, independent of callback partitioning.
    // Reading input first permits in-place processing. No allocations, locks or IO here.
    for (std::size_t i = 0; i < count; ++i) {
        float x = in ? in[i] : 0.0f;
        if (!std::isfinite(x)) { x = 0; metrics_.invalid(); }
        input_[position_] = std::clamp(x, -1.0f, 1.0f);
        if (out) out[i] = output_[position_];
        if (++position_ == input_.size()) { processFrame(); position_ = 0; }
    }
}
}
