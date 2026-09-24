#include "luna/audio.hpp"
#include "luna/wav.hpp"
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace {
volatile std::sig_atomic_t interrupted = 0;
void signalHandler(int) { interrupted = 1; }
void report(const luna::WavResult& r) {
    double seconds = double(r.samples) / luna::sample_rate;
    std::cout << std::fixed << std::setprecision(6)
      << "{\n  \"sample_rate\": 48000,\n  \"channels\": 1,\n  \"samples\": " << r.samples
      << ",\n  \"audio_seconds\": " << seconds << ",\n  \"wall_seconds\": " << r.wallSeconds
      << ",\n  \"cpu_seconds\": " << r.cpuSeconds << ",\n  \"wall_realtime_factor\": " << r.wallSeconds/seconds
      << ",\n  \"cpu_percent_one_core_equivalent\": " << r.cpuSeconds/seconds*100
      << ",\n  \"dsp_mean_us\": " << r.metrics.meanUs << ",\n  \"dsp_max_us\": " << r.metrics.maxUs
      << ",\n  \"dsp_frames_over_10ms\": " << r.metrics.lateFrames
      << ",\n  \"pipeline_delay_ms\": " << (1000.0*r.pipelineDelaySamples/luna::sample_rate)
      << ",\n  \"physical_end_to_end_latency_ms\": null,\n  \"input_rms\": "
      << r.inputRms << ",\n  \"output_rms\": " << r.outputRms << "\n}\n";
}
double number(const std::string& value) {
    std::size_t consumed = 0;
    auto x = std::stod(value, &consumed);
    if (consumed != value.size() || !std::isfinite(x)) throw std::invalid_argument("Invalid numeric option");
    return x;
}
int app(const std::vector<std::string>& args) {
    if (args.size() < 2 || args[1] == "--help") {
        std::cout << "Luna Audio AI - local noise suppression prototype\n"
          "  luna-cli devices\n"
          "  luna-cli fixture output.wav [--seconds 12]\n"
          "  luna-cli wav input.wav output.wav [--strength 0..1] [--bypass]\n"
          "  luna-cli benchmark [--seconds 60]\n"
          "  luna-cli live [--mic INDEX] [--output INDEX] [--seconds 60] [--monitor | --route]\n"
          "                [--strength 0..1] [--bypass]\n"
          "  wav/live: [--engine rnnoise | deepfilter] [--df-library PATH --df-model PATH]\n"
          "Live audio is never recorded. Monitor needs headphones. WAV output must be a new local file.\n";
        return 0;
    }
    const auto command = args[1];
    if (command != "devices" && command != "fixture" && command != "wav" && command != "benchmark" && command != "live")
        throw std::invalid_argument("Unknown command; use --help");
    int start = command == "wav" ? 4 : command == "fixture" ? 3 : 2;
    if (args.size() < static_cast<std::size_t>(start)) throw std::invalid_argument("Missing WAV path; use --help");
    double seconds = 12; float strength = 1; bool enabled = true;
    auto outputMode = luna::MonitoringOutput::Mode::muted;
    std::string engineName = "rnnoise";
    std::filesystem::path dfLibrary, dfModel;
    int mic = -1, output = -1;
    for (std::size_t i = start; i < args.size(); ++i) {
        auto option = args[i];
        if (option == "--bypass" && (command == "wav" || command == "live")) enabled = false;
        else if (option == "--monitor" && command == "live") {
            if (outputMode != luna::MonitoringOutput::Mode::muted) throw std::invalid_argument("Choose either --monitor or --route");
            outputMode = luna::MonitoringOutput::Mode::headphones;
        }
        else if (option == "--route" && command == "live") {
            if (outputMode != luna::MonitoringOutput::Mode::muted) throw std::invalid_argument("Choose either --monitor or --route");
            outputMode = luna::MonitoringOutput::Mode::route;
        }
        else if (option == "--engine" && (command == "wav" || command == "live")) {
            if (++i == args.size()) throw std::invalid_argument("Missing engine name");
            engineName = args[i];
        }
        else if ((option == "--df-library" || option == "--df-model") &&
                 (command == "wav" || command == "live")) {
            if (++i == args.size()) throw std::invalid_argument("Missing DeepFilterNet path");
            (option == "--df-library" ? dfLibrary : dfModel) = std::filesystem::u8path(args[i]);
        }
        else {
            if (++i == args.size()) throw std::invalid_argument("Missing option value");
            auto x = number(args[i]);
            if (option == "--seconds" && command != "wav" && command != "devices") seconds = x;
            else if (option == "--strength" && (command == "wav" || command == "live")) strength = static_cast<float>(x);
            else if ((option == "--mic" || option == "--output") && command == "live") {
                if (x < 0 || x > 10000 || x != std::floor(x)) throw std::invalid_argument("Device index must be a nonnegative integer");
                (option == "--mic" ? mic : output) = static_cast<int>(x);
            } else throw std::invalid_argument("Unknown/unsupported option: " + option);
        }
    }
    if (strength < 0 || strength > 1 || seconds <= 0 || seconds > 3600) throw std::invalid_argument("Invalid strength or seconds");
    if (engineName != "rnnoise" && engineName != "deepfilter") throw std::invalid_argument("Unknown engine");
    if (engineName == "deepfilter" && (dfLibrary.empty() || dfModel.empty()))
        throw std::invalid_argument("DeepFilterNet requires --df-library and --df-model");
    if (engineName == "rnnoise" && (!dfLibrary.empty() || !dfModel.empty()))
        throw std::invalid_argument("DeepFilterNet paths require --engine deepfilter");
    auto makeEngine = [&]() -> std::unique_ptr<luna::NoiseSuppressionEngine> {
        return engineName == "deepfilter" ? luna::makeDeepFilterNet3(dfLibrary, dfModel) : luna::makeRNNoise();
    };
    if (command == "fixture") { luna::generateFixture(std::filesystem::u8path(args[2]), seconds); std::cout << "Synthetic fixture created; no speech or microphone recording.\n"; return 0; }
    if (command == "wav") { report(luna::processWav(std::filesystem::u8path(args[2]), std::filesystem::u8path(args[3]), strength, enabled, makeEngine())); return 0; }
    if (command == "benchmark") { report(luna::benchmark(seconds)); return 0; }
    luna::LiveSession session;
    auto devices = session.enumerate();
    std::cout << "Backend: " << devices.backend << "\nMicrophones: " << devices.microphones.size() << '\n';
    for (std::size_t i = 0; i < devices.microphones.size(); ++i) std::cout << "  " << i << ": " << devices.microphones[i].name << '\n';
    std::cout << "Outputs: " << devices.outputs.size() << '\n';
    for (std::size_t i = 0; i < devices.outputs.size(); ++i) std::cout << "  " << i << ": " << devices.outputs[i].name << '\n';
    if (devices.microphones.empty() || devices.outputs.empty()) { std::cerr << "No usable capture/output endpoints. Physical validation is blocked.\n"; return 2; }
    if (command == "devices") return 0;
    if (mic >= static_cast<int>(devices.microphones.size()) || output >= static_cast<int>(devices.outputs.size())) throw std::invalid_argument("Device index is out of range");
    std::signal(SIGINT, signalHandler);
    session.start(mic < 0 ? nullptr : &devices.microphones[mic], output < 0 ? nullptr : &devices.outputs[output], enabled, strength, outputMode, makeEngine());
    std::cout << session.streamInfo() << "\nLive processing; recording OFF. Ctrl+C stops.\n";
    auto startTime = std::chrono::steady_clock::now();
    while (!interrupted && std::chrono::duration<double>(std::chrono::steady_clock::now()-startTime).count() < seconds) {
        if (!session.running()) throw std::runtime_error("Audio device stopped unexpectedly");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    session.stop();
    auto m = session.processor()->metrics().snapshot();
    std::cout << "DSP mean/max us: " << m.meanUs << "/" << m.maxUs << "; frames: " << m.frames
      << "; late frames: " << m.lateFrames << "; late callbacks: " << m.lateCallbacks
      << "; device events: " << session.interruptions() << '\n';
    return m.frames == 0 ? 2 : 0;
}
}
int main(int argc, char** argv) {
    try {
        std::vector<std::string> args;
#ifdef _WIN32
        // CRT argv follows the active code page. Obtain Unicode arguments directly.
        int count = 0;
        auto wide = CommandLineToArgvW(GetCommandLineW(), &count);
        if (!wide) throw std::runtime_error("Read command line failed");
        for (int i = 0; i < count; ++i) args.push_back(std::filesystem::path(wide[i]).u8string());
        LocalFree(wide);
        (void)argc; (void)argv;
#else
        args.assign(argv, argv+argc);
#endif
        return app(args);
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; }
}
