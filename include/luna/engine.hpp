#pragma once
#include <cstddef>
#include <memory>

namespace luna {
inline constexpr unsigned sample_rate = 48000;
// The host contract uses normalized float PCM. Engines own format adaptation.
class NoiseSuppressionEngine {
public:
    virtual ~NoiseSuppressionEngine() = default;
    virtual const char* name() const noexcept = 0;
    virtual unsigned sampleRate() const noexcept = 0;
    virtual std::size_t frameSize() const noexcept = 0;
    virtual std::size_t latencySamples() const noexcept = 0;
    virtual void process(const float* input, float* output) noexcept = 0;
};
std::unique_ptr<NoiseSuppressionEngine> makeRNNoise();
}
