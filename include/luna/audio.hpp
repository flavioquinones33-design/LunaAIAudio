#pragma once
#include "processor.hpp"
#include "miniaudio.h"
#include <string>
#include <vector>

namespace luna {
struct AudioDevice { std::string name; ma_device_id id{}; bool isDefault = false; };
struct DeviceList { std::vector<AudioDevice> microphones, outputs; std::string backend; };
using AudioCallback = void (*)(void*, const float*, float*, std::size_t) noexcept;
class AudioInput {
public:
    virtual ~AudioInput() = default;
    virtual DeviceList enumerate() = 0;
    virtual void start(const AudioDevice*, const AudioDevice*, AudioCallback, void*) = 0;
    virtual void stop() noexcept = 0;
    virtual bool running() const noexcept = 0;
};
class AudioOutput {
public:
    virtual ~AudioOutput() = default;
    virtual void render(float* processed, std::size_t count) noexcept = 0;
};
class MonitoringOutput final : public AudioOutput {
    // A virtual cable is a playback endpoint: it needs unity gain even when
    // headphone monitoring is disabled.
    std::atomic<float> targetGain_{0};
    float gain_ = 0;
public:
    enum class Mode { muted, headphones, route };
    void setMode(Mode mode) noexcept {
        targetGain_.store(mode == Mode::route ? 1.0f : mode == Mode::headphones ? 0.25f : 0.0f,
                          std::memory_order_relaxed);
    }
    void render(float*, std::size_t) noexcept override;
};
class MiniaudioInput final : public AudioInput {
    ma_context context_{};
    ma_device device_{};
    bool initialized_ = false;
    AudioCallback callback_ = nullptr;
    void* user_ = nullptr;
    std::atomic<unsigned> interruptions_{0};
    static void data(ma_device*, void*, const void*, ma_uint32) noexcept;
    static void notification(const ma_device_notification*) noexcept;
public:
    MiniaudioInput();
    ~MiniaudioInput() override;
    DeviceList enumerate() override;
    void start(const AudioDevice*, const AudioDevice*, AudioCallback, void*) override;
    void stop() noexcept override;
    bool running() const noexcept override;
    unsigned interruptions() const noexcept { return interruptions_.load(); }
    std::string streamInfo() const;
};
class LiveSession {
    std::unique_ptr<AudioProcessor> processor_;
    MonitoringOutput output_;
    MiniaudioInput input_;
    static void callback(void*, const float*, float*, std::size_t) noexcept;
public:
    ~LiveSession() { stop(); }
    DeviceList enumerate() { return input_.enumerate(); }
    void start(const AudioDevice*, const AudioDevice*, bool suppress, float strength,
               MonitoringOutput::Mode mode, std::unique_ptr<NoiseSuppressionEngine> engine = makeRNNoise());
    void stop() noexcept { input_.stop(); }
    bool running() const noexcept { return input_.running(); }
    unsigned interruptions() const noexcept { return input_.interruptions(); }
    std::string streamInfo() const { return input_.streamInfo(); }
    AudioProcessor* processor() noexcept { return processor_.get(); }
    MonitoringOutput& output() noexcept { return output_; }
};
}
