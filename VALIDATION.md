# Validation report — 2026-09-18

**Disposition: working offline core and cross-compiled Windows prototype; V1 physical
Windows acceptance is incomplete.** No physical microphone, Windows desktop or
representative speech corpus was available. Do not call this a production-ready
application or a Krisp-equivalent product.

## Observed evidence

| Check | Observed result | Practical implication | Confidence and scope |
|---|---|---|---|
| Native Linux Release build | Passed with GCC 13.3 / CMake 4.4.3, x86_64 | Core, CLI and tests compile and link | High for this build only |
| Windows x64 cross-build | GUI, CLI and test EXEs built with MinGW-w64 GCC 13.0.0; GUI is PE32+ Windows GUI | Downloadable Windows executable produced | High for compilation; execution on Windows remains unverified |
| Automated tests | 2/2 CTest cases; core case contains 12 test groups | Framing, scaling, bypass, mixing, files and malformed input covered | High for tested cases, not hardware behavior |
| Address/undefined behavior sanitizers | 2/2 tests passed with ASan and UBSan; UBSan configured to stop on error | No tested memory-access/undefined-behavior errors observed | Medium; finite synthetic test coverage |
| Leak checking | LeakSanitizer cannot operate under this environment's tracing restrictions | No leak-free claim; native leak profiling still required | Unavailable, explicitly not passed |
| Device enumeration | API returned one ALSA discard/zero input and output | Enumeration code executes on Linux; endpoints are synthetic | High confidence in observed list; no physical Windows evidence |
| WAV CLI integration | 12 s synthetic input -> 576,000 output samples, 48 kHz mono PCM16 | Original/processed A/B files exist and preserve converted duration | High for fixture/format; no real-speech quality claim |
| Windows runtime/UI smoke test | Wine launch blocked: `wineserver: socket: Operation not permitted` | No emulated or native Windows execution or visual QA completed | Unavailable, explicitly not passed |
| Privacy implementation review | No application network client, telemetry, live audio writer or cloud model path; file writing is explicit WAV mode | Local-only implementation with no live recording feature | High for source design; Windows packet audit pending |

Raw logs/measurements are included in `validation/`. The synthetic WAVs are included
as test assets; they contain no microphone recording or identifiable voice.

## Measured performance — one Linux server run

Test environment: Intel Xeon Platinum 8370C @ 2.80 GHz, virtualized Linux x86_64,
9 visible logical CPUs; GCC Release build. No consumer-PC extrapolation is justified.
The CPU model is the environment's report, not a hardware procurement specification.

Command: `luna-cli benchmark --seconds 60`, deterministic synthetic tones plus noise.

| Measurement | Result |
|---|---:|
| Audio processed | 60.000 s / 2,880,000 samples |
| Wall processing time | 1.992038 s |
| Real-time factor | 0.033201 |
| Mean inference + mixing time per 10 ms frame | 0.299339 ms |
| Maximum observed frame time | 8.349197 ms |
| Frames exceeding 10 ms | 0 in this run |
| CPU-time equivalent per source duration | 3.313485% of one core |
| Fixed software pipeline delay | 20 ms |
| Physical end-to-end latency | Not measured |

**Interpretation:** this fixture ran faster than real time with average compute
headroom on the tested server. Confidence is **high** for the recorded measurement,
**low** for predicting latency, dropouts or CPU on an ordinary Windows PC. The maximum
is materially above the mean; scheduling/host contention may contribute, but the
cause was not isolated. A longer native live run is necessary.

The CPU-time equivalent includes synthetic-signal generation and processing. It is
not Task Manager utilization during the accelerated benchmark. Live UI CPU uses
actual process CPU time per wall time instead. The benchmark has no audio hardware
or real-time delivery deadline enforced by a driver.

## WAV comparison

`validation/wav-linux.json` reports a 12-second WAV processed in 0.378046 seconds,
with mean DSP time 0.297377 ms and output length 576,000 samples. The processor trims
its known delay and flushes EOF. Automated bypass tests also cover lengths 1, 17,
479, 480, 481, 1001 and 48,000 samples, including the final nonzero sample.

Measured RMS noise-energy attenuation in four predefined noise-only half-second
windows of the synthetic fixture was **11.085 dB**. Window sample indices are in
`validation/wav-comparison.json`. Confidence is **high** for this calculation on
these files; it provides **no evidence** of speech intelligibility, target-speaker
isolation or performance against any commercial product. Harmonic tones are not
natural speech. Lower output RMS can also result from speech damage in real clips.

## Automated coverage

1. Silence, finite RNNoise output and scalar metrics.
2. Exact parity with upstream RNNoise after float/PCM amplitude conversion.
3. Bypass preservation, arbitrary callback chunks and EOF tail.
4. Dry/wet latency alignment with alternative frame sizes/delays.
5. RNNoise output independent of callback partitioning.
6. NaN/Inf sanitization, amplitude bounds and in-place processing.
7. Repeated enabled/mix transitions stay finite and bounded.
8. Reduction of deterministic stationary-noise energy (not a speech-quality test).
9. Short/partial/whole-frame WAV lengths and PCM16 tolerance.
10. 44.1 kHz stereo -> 48 kHz mono conversion.
11. Protection of originals/existing outputs; missing, malformed, truncated and
    empty WAV rejection; handled-failure output cleanup.
12. Invalid engine metadata, fixture duration and WAV strength rejection.

No automated microphone test is silently substituted with a null device. Normal
tests never open a microphone. Continuous listening, clock drift, physical xrun
behavior, disconnects, suspend/resume, native MSVC and UI layout/accessibility
remain untested on Windows.

## Completion gate

Execute [WINDOWS_TEST_CHECKLIST.md](WINDOWS_TEST_CHECKLIST.md) on the target Windows
PC. Record results, hardware, test duration, adverse findings and any fixes before
marking V1 complete. [V2_ROADMAP.md](V2_ROADMAP.md) remains a conditional proposal.
