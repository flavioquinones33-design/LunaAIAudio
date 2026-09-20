# Technical Notes, Validation Status & Third-Party Notices

This document preserves the technical detail, validation status, and licensing
information that was part of an earlier, more detailed README. The current
user-facing README.md is intentionally lighter; this file is the reference for
build reproducibility, known limitations, and upstream attributions.

## Build status

**Status: V1 prototype / hardware validation pending.** The audio core is tested
on Linux; Windows executables in the current delivery were produced via a
**cross-compile from Linux using MinGW-w64**, not a native MSVC build. This is
not a completed Windows hardware acceptance test. No claim of Krisp-equivalent
performance is made.

MSVC build instructions (below) are provided for reproducibility, but native
MSVC acceptance on physical Windows hardware remains pending.

## Exact Windows source build (Visual Studio 2022)

Prerequisites:

- Windows 10/11 x64.
- Visual Studio 2022 or Build Tools 2022, with **Desktop development with C++**,
  MSVC v143 x64/x86 tools and a Windows 10 or 11 SDK.
- CMake 3.20 or newer on PATH. The Visual Studio CMake tools component is suitable.

No Python, npm, model download, package restore or Internet connection is needed
to build after the toolchain is installed. The dependencies and model are vendored.

Open **Developer PowerShell for VS 2022**, enter the extracted source folder
containing `CMakeLists.txt`, and run exactly:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\build\Release\luna-cli.exe devices
.\build\Release\LunaAudioAI.exe
```

The desktop app is `build\Release\LunaAudioAI.exe`; command-line tools are
`luna-cli.exe` and `luna-tests.exe` in the same folder. The runtime is linked
statically. Copy the notices and documentation with any redistributed binaries.

## Developer build on Linux

The portable core/CLI/tests also build on Linux; the GUI is Windows-only:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/luna-cli benchmark --seconds 60
```

For a Windows x64 cross-build with MinGW-w64 installed:

```bash
cmake -S . -B build-windows -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-x64.cmake
cmake --build build-windows --parallel
```

Memory checks: configure a separate Debug build with `-DLUNA_SANITIZE=ON` using
GCC or Clang, then run CTest. Tests create synthetic WAVs in a temporary
directory and remove them on normal completion; they do not open a microphone
or need a network.

## What the controls and metrics mean

| Display/control | Meaning |
|---|---|
| Noise suppression OFF | Delayed original signal; RNNoise stays warm for smooth switching. CPU does not drop to zero. |
| Suppression mix | 0% delayed dry, 100% RNNoise; delay-aligned blend, not an RNNoise model strength parameter. |
| Input/output meters | Latest 10 ms RMS in dBFS, floor -120 dBFS. Output is measured before monitoring attenuation/mute. Red indicates peak >= 0.99. |
| DSP last / mean / max | Measured wall time for RNNoise + dry/wet mixing per frame, since the current Start. Not acoustic round-trip latency. |
| Pipeline delay | 480-sample frame adapter + 480-sample RNNoise overlap delay = 20 ms. Hardware/backend buffering is additional. |
| Process CPU | Actual process CPU time, normalized to one core; includes UI and audio. Updates about once a second. |
| Late frames / callbacks | Processing wall time exceeded the relevant audio duration. Not a complete WASAPI xrun counter. |
| CLI real-time factor | Processing wall seconds / source audio seconds. Below 1 is faster than real time on the tested machine. |

Physical end-to-end latency is explicitly **unmeasured** until a hardware
loopback test. RNNoise algorithm delay and processing execution time are
different quantities. The requested backend period is 10 ms, two periods; the
driver may select others.

## Known limitations and next gate

- Physical Windows enumeration, capture, playback, long-run stability and
  native MSVC execution must still be signed off (see WINDOWS_TEST_CHECKLIST.md).
- RNNoise v0.1 is a deliberately pinned older baseline, not the newest RNNoise.
  The distributed upstream model is embedded unchanged; no proprietary model is used.
- No acoustic echo cancellation, target-speaker isolation, background-voice
  removal, auto noise classification, virtual microphone or meeting-app
  integration. There is no virtual microphone in V1: the app does not appear
  as a microphone choice in Zoom, Google Meet or Teams.
- Speech quality and artifact rates have not been established on a
  representative speech/noise dataset. Noise energy reduction alone does not
  measure intelligibility.
- Different capture/playback clocks, USB reconnection, suspend/resume,
  exclusive devices and Bluetooth need hardware testing. Restart after a
  device failure. Bluetooth profiles and virtual devices are not yet validated.
- The native GUI has a fixed compact layout with startup DPI scaling;
  per-monitor DPI changes and full screen-reader accessibility are not yet
  validated.
- The app is unsigned and has no installer. No benchmark here establishes
  typical consumer-PC CPU usage or Krisp-equivalent performance.

See V2_ROADMAP.md for proposed work, gated on completing V1 hardware validation.

## Data handling caveat

Do not use identifiable patient audio or confidential clinical audio for
prototype testing without organizational authorization for that specific
workflow.

## Upstream sources & third-party notices

- **RNNoise** — [official source](https://github.com/xiph/rnnoise), pinned tag
  v0.1, commit `cdf196b1e9de2f8ff1003328ebf9a4316477429d`. RNNoise remains the
  work of its respective authors and contributors.
- **miniaudio** — [manual](https://miniaud.io/docs/manual/index.html), vendored
  release 0.11.23. File hashes are in `third_party/MANIFEST.json`.
- Full licenses and attributions: see THIRD_PARTY_NOTICES.md.

Copy this notices file and related documentation along with any redistributed
binaries.
