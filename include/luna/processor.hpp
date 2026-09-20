#pragma once
#include "engine.hpp"
#include "metrics.hpp"
#include <atomic>
#include <vector>

namespace luna {
class AudioProcessor {
    std::unique_ptr<NoiseSuppressionEngine> engine_;
    std::vector<float> input_, output_, wet_, dry_, delay_;
    std::size_t position_ = 0, delayPosition_ = 0;
    std::atomic<bool> enabled_{true};
    std::atomic<float> strength_{1};
    float mix_ = 1;
    AudioMetrics metrics_;
    void processFrame() noexcept;
public:
    explicit AudioProcessor(std::unique_ptr<NoiseSuppressionEngine> engine = makeRNNoise());
    // Initial controls before the first sample are applied without a fade.
    // During a stream, controls ramp across one frame to reduce discontinuities.
    void setEnabled(bool enabled) noexcept { enabled_.store(enabled, std::memory_order_relaxed); }
    void setStrength(float strength) noexcept;
    void process(const float* input, float* output, std::size_t count) noexcept;
    std::size_t latencySamples() const noexcept { return input_.size() + engine_->latencySamples(); }
    std::size_t frameSize() const noexcept { return input_.size(); }
    AudioMetrics& metrics() noexcept { return metrics_; }
    const AudioMetrics& metrics() const noexcept { return metrics_; }
};
}
