#pragma once
#include <cstddef>
#include <memory>
#include <filesystem>

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
// Loads the upstream DeepFilterNet C ABI and a separately supplied DFN3 ONNX model.
// Neither the model nor the native library is bundled with Luna.
std::unique_ptr<NoiseSuppressionEngine> makeDeepFilterNet3(
    const std::filesystem::path& library, const std::filesystem::path& model);
}
