#include "luna/wav.hpp"
#include "miniaudio.h"
#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <stdexcept>
#include <fcntl.h>
#include <fstream>
#ifdef _WIN32
#include <io.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#endif

namespace luna {
namespace {
void validateContainer(const std::filesystem::path& path) {
    std::ifstream f(path,std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open input WAV");
    const auto size=std::filesystem::file_size(path);
    std::array<unsigned char,12> head{};
    f.read(reinterpret_cast<char*>(head.data()),head.size());
    auto u32=[](const unsigned char* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);};
    if (!f || !std::equal(head.begin(),head.begin()+4,"RIFF") || !std::equal(head.begin()+8,head.end(),"WAVE"))
        throw std::runtime_error("Expected a RIFF WAV file (RF64/RIFX unsupported)");
    std::uint64_t end=std::uint64_t(u32(head.data()+4))+8;
    if (end>size || end<12) throw std::runtime_error("Truncated/invalid RIFF length");
    for (std::uint64_t pos=12;pos<end;) {
        if (end-pos<8) throw std::runtime_error("Truncated WAV chunk header");
        std::array<unsigned char,8> chunk{};f.seekg(static_cast<std::streamoff>(pos));
        f.read(reinterpret_cast<char*>(chunk.data()),chunk.size());
        auto n=u32(chunk.data()+4);
        if (!f || std::uint64_t(n)>end-pos-8) throw std::runtime_error("Truncated WAV chunk");
        pos+=8+std::uint64_t(n)+(n&1);
        if(pos>end) throw std::runtime_error("Missing WAV chunk padding");
    }
}
void le16(unsigned char* p, std::uint16_t x) { p[0] = static_cast<unsigned char>(x); p[1] = static_cast<unsigned char>(x >> 8); }
void le32(unsigned char* p, std::uint32_t x) { for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(x >> (8*i)); }
float fixtureSample(std::uint64_t i, std::uint32_t& state) {
    state = 1664525u * state + 1013904223u;
    float noise = (static_cast<float>(state >> 8) / 8388608.0f - 1.0f) * 0.055f;
    double t = static_cast<double>(i) / sample_rate;
    // Synthetic tones + noise, with noise-only first/last second in each 6 s cycle.
    double envelope = std::fmod(t, 6.0) > 1 && std::fmod(t, 6.0) < 5 ? 0.5 - 0.5*std::cos(2*3.141592653589793*t*3) : 0;
    return noise + static_cast<float>(envelope * (0.16*std::sin(2*3.141592653589793*180*t) + 0.08*std::sin(2*3.141592653589793*540*t)));
}
void validateSeconds(double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0 || seconds > 3600)
        throw std::invalid_argument("Seconds must be in (0, 3600]");
}
class Decoder {
public:
    ma_decoder value{};
    explicit Decoder(const std::filesystem::path& path) {
        validateContainer(path);
        auto cfg = ma_decoder_config_init(ma_format_f32, 1, sample_rate);
        cfg.encodingFormat = ma_encoding_format_wav;
#ifdef _WIN32
        auto result = ma_decoder_init_file_w(path.c_str(), &cfg, &value);
#else
        auto result = ma_decoder_init_file(path.c_str(), &cfg, &value);
#endif
        if (result != MA_SUCCESS) throw std::runtime_error(std::string("Read WAV: ") + ma_result_description(result));
    }
    ~Decoder() { ma_decoder_uninit(&value); }
};
}
WavWriter::WavWriter(const std::filesystem::path& path) : path_(path) {
    // Exclusive creation prevents truncating originals, existing outputs, or symlink targets.
#ifdef _WIN32
    int fd = _wopen(path.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
    if (fd >= 0) { file_ = _fdopen(fd, "wb"); if (!file_) _close(fd); }
#else
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd >= 0) { file_ = fdopen(fd, "wb"); if (!file_) ::close(fd); }
#endif
    if (!file_) throw std::runtime_error("Cannot create output: choose a new local filename in an existing folder");
    std::array<unsigned char, 44> header{};
    if (std::fwrite(header.data(), 1, header.size(), file_) != header.size()) {
        std::fclose(file_); file_ = nullptr;
        std::error_code error; std::filesystem::remove(path_, error);
        throw std::runtime_error("Cannot write WAV header");
    }
}
WavWriter::~WavWriter() {
    if (file_) std::fclose(file_);
    if (!finished_) { std::error_code error; std::filesystem::remove(path_, error); }
}
void WavWriter::write(const float* data, std::size_t count) {
    if (finished_ || !file_) throw std::runtime_error("WAV is closed");
    if (samples_ + count > (0xffffffffULL - 36) / 2) throw std::runtime_error("Output exceeds RIFF WAV size limit");
    std::array<unsigned char, 8192> buffer{};
    while (count) {
        auto n = std::min(count, buffer.size()/2);
        for (std::size_t i = 0; i < n; ++i) {
            auto v = std::isfinite(data[i]) ? std::clamp(data[i], -1.0f, 1.0f) : 0;
            auto pcm = static_cast<std::int16_t>(std::lround(v < 0 ? v*32768 : v*32767));
            le16(buffer.data()+i*2, static_cast<std::uint16_t>(pcm));
        }
        if (std::fwrite(buffer.data(), 2, n, file_) != n) throw std::runtime_error("WAV write failed (disk full?)");
        data += n; count -= n; samples_ += n;
    }
}
void WavWriter::finish() {
    if (finished_ || !file_) throw std::runtime_error("WAV is closed");
    std::array<unsigned char, 44> h{};
    std::copy_n("RIFF", 4, h.data()); le32(h.data()+4, static_cast<std::uint32_t>(36 + samples_*2));
    std::copy_n("WAVEfmt ", 8, h.data()+8); le32(h.data()+16, 16);
    le16(h.data()+20, 1); le16(h.data()+22, 1); le32(h.data()+24, sample_rate);
    le32(h.data()+28, sample_rate*2); le16(h.data()+32, 2); le16(h.data()+34, 16);
    std::copy_n("data", 4, h.data()+36); le32(h.data()+40, static_cast<std::uint32_t>(samples_*2));
    if (std::fseek(file_, 0, SEEK_SET) != 0 || std::fwrite(h.data(), 1, h.size(), file_) != h.size())
        throw std::runtime_error("Finalize WAV header failed");
    int result = std::fclose(file_); file_ = nullptr;
    if (result != 0) throw std::runtime_error("Flush WAV failed");
    finished_ = true;
}
WavResult processWav(const std::filesystem::path& input, const std::filesystem::path& output, float strength, bool enabled) {
    if (!std::isfinite(strength) || strength < 0 || strength > 1) throw std::invalid_argument("Strength must be 0..1");
    Decoder decoder(input);
    AudioProcessor processor;
    processor.setStrength(strength); processor.setEnabled(enabled);
    WavWriter writer(output);
    std::array<float, 4096> in{}, out{};
    auto skip = processor.latencySamples();
    WavResult r;
    double inputEnergy = 0, outputEnergy = 0;
    auto write = [&](std::size_t count) {
        auto offset = std::min(skip, count); skip -= offset;
        writer.write(out.data()+offset, count-offset);
        for (std::size_t i = offset; i < count; ++i) outputEnergy += double(out[i])*out[i];
    };
    auto started = std::chrono::steady_clock::now(); auto cpuStart = processCpuSeconds();
    for (;;) {
        ma_uint64 count = 0;
        auto status = ma_decoder_read_pcm_frames(&decoder.value, in.data(), in.size(), &count);
        if (status != MA_SUCCESS && status != MA_AT_END) throw std::runtime_error("WAV decoding failed");
        if (count == 0) break;
        for (std::size_t i = 0; i < count; ++i) {
            if (!std::isfinite(in[i])) throw std::runtime_error("WAV contains non-finite samples");
            inputEnergy += double(in[i])*in[i];
        }
        processor.process(in.data(), out.data(), static_cast<std::size_t>(count));
        r.samples += count;
        write(static_cast<std::size_t>(count));
    }
    if (!r.samples) throw std::runtime_error("WAV has no audio frames");
    // Flush the entire adapter + engine delay, and trim leading delay for aligned A/B.
    std::size_t flush = processor.latencySamples();
    while (flush) { auto n = std::min(flush, out.size()); processor.process(nullptr, out.data(), n); write(n); flush -= n; }
    writer.finish();
    r.wallSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
    r.cpuSeconds = processCpuSeconds()-cpuStart;
    r.inputRms = std::sqrt(inputEnergy/r.samples); r.outputRms = std::sqrt(outputEnergy/r.samples);
    r.metrics = processor.metrics().snapshot();
    return r;
}
void generateFixture(const std::filesystem::path& output, double seconds) {
    validateSeconds(seconds);
    WavWriter writer(output); std::uint32_t state = 17;
    std::array<float, 480> frame{};
    auto n = static_cast<std::uint64_t>(seconds * sample_rate);
    for (std::uint64_t i = 0; i < n;) {
        auto count = static_cast<std::size_t>(std::min<std::uint64_t>(frame.size(), n-i));
        for (std::size_t j = 0; j < count; ++j) frame[j] = fixtureSample(i+j, state);
        writer.write(frame.data(), count); i += count;
    }
    writer.finish();
}
WavResult benchmark(double seconds) {
    validateSeconds(seconds); AudioProcessor processor;
    std::array<float, 480> in{}, out{}; std::uint32_t state = 17;
    WavResult r; r.samples = static_cast<std::uint64_t>(seconds * sample_rate);
    double inputEnergy=0,outputEnergy=0;
    auto start = std::chrono::steady_clock::now(); auto cpuStart = processCpuSeconds();
    for (std::uint64_t i = 0; i < r.samples;) {
        auto n = static_cast<std::size_t>(std::min<std::uint64_t>(480, r.samples-i));
        for (std::size_t j = 0; j < n; ++j) in[j] = fixtureSample(i+j, state);
        processor.process(in.data(), out.data(), n);
        for(std::size_t j=0;j<n;++j){inputEnergy+=double(in[j])*in[j];outputEnergy+=double(out[j])*out[j];}
        i += n;
    }
    r.wallSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    r.cpuSeconds = processCpuSeconds()-cpuStart;
    r.inputRms=std::sqrt(inputEnergy/r.samples);r.outputRms=std::sqrt(outputEnergy/r.samples);
    r.metrics = processor.metrics().snapshot(); return r;
}
}
