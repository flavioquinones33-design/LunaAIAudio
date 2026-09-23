#include "luna/audio.hpp"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <stdexcept>

namespace luna {
namespace {
void check(ma_result result, const char* action) {
    if (result != MA_SUCCESS) throw std::runtime_error(std::string(action) + ": " + ma_result_description(result));
}
}
MiniaudioInput::MiniaudioInput() {
#ifdef _WIN32
    ma_backend backend = ma_backend_wasapi;
    check(ma_context_init(&backend, 1, nullptr, &context_), "Initialize Windows WASAPI");
#else
    check(ma_context_init(nullptr, 0, nullptr, &context_), "Initialize audio backend");
#endif
}
MiniaudioInput::~MiniaudioInput() { stop(); ma_context_uninit(&context_); }
DeviceList MiniaudioInput::enumerate() {
    if (initialized_) throw std::runtime_error("Stop audio before refreshing devices");
    ma_device_info *out = nullptr, *in = nullptr;
    ma_uint32 no = 0, ni = 0;
    check(ma_context_get_devices(&context_, &out, &no, &in, &ni), "Enumerate devices");
    DeviceList list;
    list.backend = ma_get_backend_name(context_.backend);
    for (ma_uint32 i = 0; i < ni; ++i) list.microphones.push_back({in[i].name, in[i].id, in[i].isDefault != 0});
    for (ma_uint32 i = 0; i < no; ++i) list.outputs.push_back({out[i].name, out[i].id, out[i].isDefault != 0});
    return list;
}
void MiniaudioInput::data(ma_device* device, void* out, const void* in, ma_uint32 count) noexcept {
    auto& self = *static_cast<MiniaudioInput*>(device->pUserData);
    self.callback_(self.user_, static_cast<const float*>(in), static_cast<float*>(out), count);
}
void MiniaudioInput::notification(const ma_device_notification* event) noexcept {
    auto& self = *static_cast<MiniaudioInput*>(event->pDevice->pUserData);
    if (event->type == ma_device_notification_type_rerouted ||
        event->type == ma_device_notification_type_interruption_began)
        self.interruptions_.fetch_add(1, std::memory_order_relaxed);
}
void MiniaudioInput::start(const AudioDevice* microphone, const AudioDevice* output,
                          AudioCallback callback, void* user) {
    stop();
    if (!callback) throw std::invalid_argument("Missing audio callback");
    callback_ = callback; user_ = user; interruptions_.store(0);
    auto config = ma_device_config_init(ma_device_type_duplex);
    config.capture.pDeviceID = microphone ? &microphone->id : nullptr;
    config.playback.pDeviceID = output ? &output->id : nullptr;
    config.capture.format = config.playback.format = ma_format_f32;
    config.capture.channels = config.playback.channels = 1;
    config.sampleRate = sample_rate;
    config.periodSizeInFrames = 480;
    config.periods = 2;
    config.noFixedSizedCallback = MA_TRUE; // processor accepts arbitrary callback partitions
    config.performanceProfile = ma_performance_profile_low_latency;
    config.dataCallback = data; config.notificationCallback = notification; config.pUserData = this;
    check(ma_device_init(&context_, &config, &device_), "Open microphone/output (check Windows permissions)");
    initialized_ = true;
    auto result = ma_device_start(&device_);
    if (result != MA_SUCCESS) { stop(); check(result, "Start audio device"); }
}
void MiniaudioInput::stop() noexcept {
    if (initialized_) { ma_device_uninit(&device_); initialized_ = false; }
    callback_ = nullptr; user_ = nullptr;
}
bool MiniaudioInput::running() const noexcept {
    return initialized_ && ma_device_is_started(&device_);
}
std::string MiniaudioInput::streamInfo() const {
    if (!initialized_) return "Audio stopped";
    std::ostringstream s;
    s << "48 kHz mono | Native capture " << device_.capture.internalSampleRate
      << " Hz / " << device_.capture.internalChannels << " ch | Native output "
      << device_.playback.internalSampleRate << " Hz / " << device_.playback.internalChannels << " ch";
    return s.str();
}
void MonitoringOutput::render(float* samples, std::size_t count) noexcept {
    if (!samples) return;
    // Headphones are quieter (-12 dB); routed output is unity gain.
    float target = targetGain_.load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < count; ++i) {
        gain_ += std::clamp(target - gain_, -1.0f/240, 1.0f/240);
        samples[i] *= gain_;
    }
}
void LiveSession::callback(void* user, const float* in, float* out, std::size_t count) noexcept {
    auto& self = *static_cast<LiveSession*>(user);
    auto start = std::chrono::steady_clock::now();
    self.processor_->process(in, out, count);
    self.output_.render(out, count);
    float us = std::chrono::duration<float, std::micro>(std::chrono::steady_clock::now() - start).count();
    self.processor_->metrics().callback(us, static_cast<float>(count) * 1000000.0f / sample_rate);
}
void LiveSession::start(const AudioDevice* mic, const AudioDevice* out, bool suppress, float strength, MonitoringOutput::Mode mode) {
    stop();
    processor_ = std::make_unique<AudioProcessor>();
    processor_->setEnabled(suppress); processor_->setStrength(strength);
    output_.setMode(mode);
    input_.start(mic, out, callback, this);
}
}
