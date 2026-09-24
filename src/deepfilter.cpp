#include "luna/engine.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace luna {
namespace {
struct DFState;
#ifdef _WIN32
using Module = HMODULE;
Module openModule(const std::filesystem::path& path) {
    return LoadLibraryExW(path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
}
void* symbol(Module m, const char* name) { return reinterpret_cast<void*>(GetProcAddress(m, name)); }
void closeModule(Module m) { FreeLibrary(m); }
#else
using Module = void*;
Module openModule(const std::filesystem::path& path) { return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL); }
void* symbol(Module m, const char* name) { return dlsym(m, name); }
void closeModule(Module m) { dlclose(m); }
#endif

class DeepFilterNet3Engine final : public NoiseSuppressionEngine {
    using Create = DFState* (*)(const char*, float, const char*);
    using FrameLength = std::size_t (*)(DFState*);
    using Process = float (*)(DFState*, float*, float*);
    using Free = void (*)(DFState*);
    Module module_ = nullptr;
    DFState* state_ = nullptr;
    Free free_ = nullptr;
    Process process_ = nullptr;
    std::array<float, 480> input_{};
public:
    DeepFilterNet3Engine(const std::filesystem::path& library, const std::filesystem::path& model) {
        if (!std::filesystem::is_regular_file(library) || !std::filesystem::is_regular_file(model))
            throw std::runtime_error("DeepFilterNet3 requires a native C API library and model archive; see README");
        if (model.filename() != "DeepFilterNet3_onnx.tar.gz")
            throw std::runtime_error("Select the official DeepFilterNet3_onnx.tar.gz model (48 kHz, 480-sample hop)");
        auto absoluteLibrary = std::filesystem::absolute(library);
        module_ = openModule(absoluteLibrary);
        if (!module_) throw std::runtime_error("Could not load the DeepFilterNet C API library or its dependencies");
        try {
            auto create = reinterpret_cast<Create>(symbol(module_, "df_create"));
            auto length = reinterpret_cast<FrameLength>(symbol(module_, "df_get_frame_length"));
            process_ = reinterpret_cast<Process>(symbol(module_, "df_process_frame"));
            free_ = reinterpret_cast<Free>(symbol(module_, "df_free"));
            if (!create || !length || !process_ || !free_)
                throw std::runtime_error("Library does not export the official DeepFilterNet C API");
            // Upstream df_create may abort on corrupt/untrusted model archives. Supply only
            // the unmodified archive from the pinned upstream release.
            state_ = create(std::filesystem::absolute(model).u8string().c_str(), 100.0f, nullptr);
            if (!state_ || length(state_) != input_.size())
                throw std::runtime_error("DeepFilterNet3 model must run at 48 kHz with 480 samples per frame");
        } catch (...) {
            if (state_ && free_) free_(state_);
            closeModule(module_);
            throw;
        }
    }
    ~DeepFilterNet3Engine() override { free_(state_); closeModule(module_); }
    const char* name() const noexcept override { return "DeepFilterNet3"; }
    unsigned sampleRate() const noexcept override { return sample_rate; }
    std::size_t frameSize() const noexcept override { return 480; }
    // fft_size - hop_size (480) + two 480-sample lookahead frames.
    // The AudioProcessor adds a separate frame-adapter delay.
    std::size_t latencySamples() const noexcept override { return 1440; }
    void process(const float* in, float* out) noexcept override {
        for (std::size_t i = 0; i < input_.size(); ++i) input_[i] = in[i];
        process_(state_, input_.data(), out);
        for (std::size_t i = 0; i < input_.size(); ++i)
            out[i] = std::isfinite(out[i]) ? std::clamp(out[i], -1.0f, 1.0f) : 0.0f;
    }
};
}
std::unique_ptr<NoiseSuppressionEngine> makeDeepFilterNet3(
    const std::filesystem::path& library, const std::filesystem::path& model) {
    return std::make_unique<DeepFilterNet3Engine>(library, model);
}
}
