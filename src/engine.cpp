#include "luna/engine.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <stdexcept>
extern "C" {
#include "rnnoise.h"
}

namespace luna {
namespace {
class RNNoiseEngine final : public NoiseSuppressionEngine {
    DenoiseState* state_ = nullptr;
    std::array<float, 480> scaled_{}, result_{};
public:
    RNNoiseEngine() {
        // v0.1 lazily initializes shared FFT tables. Warm them once, off the callback.
        static std::once_flag init;
        std::call_once(init, [] {
            auto* warm = rnnoise_create();
            if (!warm) throw std::runtime_error("RNNoise allocation failed");
            std::array<float, 480> zero{};
            rnnoise_process_frame(warm, zero.data(), zero.data());
            rnnoise_destroy(warm);
        });
        state_ = rnnoise_create();
        if (!state_) throw std::runtime_error("RNNoise allocation failed");
    }
    ~RNNoiseEngine() override { rnnoise_destroy(state_); }
    const char* name() const noexcept override { return "RNNoise v0.1"; }
    unsigned sampleRate() const noexcept override { return sample_rate; }
    std::size_t frameSize() const noexcept override { return 480; }
    std::size_t latencySamples() const noexcept override { return 480; }
    void process(const float* in, float* out) noexcept override {
        for (std::size_t i = 0; i < 480; ++i) scaled_[i] = in[i] * 32768.0f;
        rnnoise_process_frame(state_, result_.data(), scaled_.data());
        for (std::size_t i = 0; i < 480; ++i) {
            float x = result_[i] / 32768.0f;
            out[i] = std::isfinite(x) ? std::clamp(x, -1.0f, 1.0f) : 0;
        }
    }
};
}
std::unique_ptr<NoiseSuppressionEngine> makeRNNoise() { return std::make_unique<RNNoiseEngine>(); }
}
