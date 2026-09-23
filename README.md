# Luna Audio AI

Local microphone noise suppression for Windows x64. C++17, a native Win32 desktop UI,
WASAPI through miniaudio, and the original open-source RNNoise v0.1 model.

**Status: V1 prototype / hardware validation pending.** The audio core is tested on
Linux and Windows executables are cross-compiled. This is not a completed Windows
hardware acceptance test. See [VALIDATION.md](VALIDATION.md) for actual results,
confidence and remaining gates. No claim of Krisp-equivalent performance is made.

## Run the supplied Windows prototype

1. Extract the entire `Luna_Audio_AI_Windows_x64.zip` into a local folder.
2. Open `LunaAudioAI.exe` on Windows 10/11 x64. The prototype is unsigned.
3. Select a microphone and a headphone/output endpoint. Choose **Headphones (-12 dB)**
   in the output mode selector if you want to hear the processed signal, then click
   **Start audio**. The output mode defaults to **Muted**.
5. Toggle **Noise suppression**, or adjust **Suppression mix**. Click **Stop audio**
   before changing devices. Closing the app stops capture and releases the device.

### Send processed audio to a call or recording app

The application has no built-in virtual microphone. For app-to-app routing, install a
virtual audio cable separately, for example VB-CABLE from its official site
<https://vb-audio.com/Cable/>. Follow its installation and reboot instructions.
The driver is not bundled with Luna Audio AI. Then:

1. In Luna Audio AI, choose your **physical microphone** as input and **CABLE Input**
   (the cable's *playback* endpoint) as output. Click **Refresh** if it is missing.
2. Set output mode to **Virtual cable (full level)**, enable noise suppression,
   set the suppression mix to 100%, and click **Start audio**.
3. In the calling/recording app, select **CABLE Output** (the cable's *recording*
   endpoint) as its microphone. Do not select the physical microphone there.
4. Make a short test recording in the calling app, explicitly if you want to record.
   Switch Luna's suppression OFF/ON to compare. Luna itself does not record live audio.

Do not select the cable's recording endpoint as Luna's input: that can create a loop.
Do not set CABLE Input as Windows' default speaker; keep other computer audio on
your normal headphones/speakers. Routing to a physical speaker at full level can
create feedback, so use the **Headphones (-12 dB)** mode for direct listening.
Both capture and playback endpoints are required even when the output mode is muted.
The Windows driver and actual call app routing still require hardware validation.

Windows microphone access must be enabled for desktop apps in **Settings > Privacy
& security > Microphone** (Windows 11) or **Settings > Privacy > Microphone** (Windows
10). If capture fails, verify the selected device, permission and exclusive use by
another application, then stop, refresh and restart. Bluetooth profiles and virtual
devices are not yet validated. Use wired headphones for initial tests.

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
MSVC instructions are supplied for reproducibility; the executed Windows compiler
in this delivery was MinGW-w64. Native MSVC acceptance remains pending.

## WAV comparison without a microphone

From the source build, use a new output folder on every run:

```powershell
$run = Join-Path .\out (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
New-Item -ItemType Directory -Path $run -Force | Out-Null
.\build\Release\luna-cli.exe fixture "$run\original.wav" --seconds 12
.\build\Release\luna-cli.exe wav "$run\original.wav" "$run\denoised.wav"
.\build\Release\luna-cli.exe wav "$run\original.wav" "$run\bypass.wav" --bypass
.\build\Release\luna-cli.exe wav "$run\original.wav" "$run\mix50.wav" --strength 0.5
.\build\Release\luna-cli.exe benchmark --seconds 60
```

With the portable package, replace `.\build\Release\luna-cli.exe` with
`.\luna-cli.exe`. `samples` contains an already generated synthetic A/B pair.

`fixture` makes deterministic tones and noise, not human speech. For a meaningful
listening test, use an authorized, nonconfidential WAV with speech plus noise:

```powershell
.\build\Release\luna-cli.exe wav "C:\AudioTests\noisy.wav" "C:\AudioTests\denoised-new.wav"
```

Only WAV processing explicitly writes audio. Existing files are never overwritten.
Input is decoded/downmixed/resampled locally to 48 kHz mono; output is 16-bit PCM
WAV. Leading pipeline delay is trimmed and the end is flushed, preserving the
converted input length for aligned comparisons. Expect normal 16-bit quantization.
Use local, unsynced folders. RF64/RIFX and files exceeding standard RIFF output
limits are unsupported. Truncated RIFF containers and empty WAVs are rejected.

## Live command-line test

```powershell
.\build\Release\luna-cli.exe devices
.\build\Release\luna-cli.exe live --mic 0 --output 0 --seconds 600
# Explicit headphone monitoring:
.\build\Release\luna-cli.exe live --mic 0 --output 0 --seconds 60 --monitor
# Route to an explicitly selected virtual cable playback endpoint:
.\build\Release\luna-cli.exe live --mic 0 --output 1 --seconds 60 --route
```

Use indices from `devices`; indices can change when devices are reconnected.
The `--output 1` above is only an example: check your own device list for CABLE Input.
Ctrl+C stops. `live` never records. Omitting an index selects the system default.
The GUI exposes the same engine and streaming processor.

## What the controls and metrics mean

| Display/control | Meaning |
|---|---|
| Noise suppression OFF | Delayed original signal; RNNoise stays warm for smooth switching. CPU does not drop to zero. |
| Output mode | Muted by default; Headphones outputs at -12 dB; Virtual cable outputs processed audio at unity gain to the selected playback endpoint. Only a separate cable driver exposes this as a recording endpoint to other apps. |
| Suppression mix | 0% delayed dry, 100% RNNoise; delay-aligned blend, not an RNNoise model strength parameter. |
| Input/output meters | Latest 10 ms RMS in dBFS, floor -120 dBFS. Output is measured before monitoring attenuation/mute. Red indicates peak >= 0.99. |
| DSP last / mean / max | Measured wall time for RNNoise + dry/wet mixing per frame, since the current Start. Not acoustic round-trip latency. |
| Pipeline delay | 480-sample frame adapter + 480-sample RNNoise overlap delay = 20 ms. Hardware/backend buffering is additional. |
| Process CPU | Actual process CPU time, normalized to one core; includes UI and audio. Updates about once a second. |
| Late frames / callbacks | Processing wall time exceeded the relevant audio duration. Not a complete WASAPI xrun counter. |
| CLI real-time factor | Processing wall seconds / source audio seconds. Below 1 is faster than real time on the tested machine. |

Physical end-to-end latency is explicitly **unmeasured** until a hardware loopback
test. RNNoise algorithm delay and processing execution time are different quantities.
The requested backend period is 10 ms, two periods; the driver may select others.

## Architecture and privacy

See [ARCHITECTURE.md](ARCHITECTURE.md) and [PRIVACY.md](PRIVACY.md).

The app has no network client, cloud processing, account, update checker, telemetry,
audio upload, live recorder or transcription. Runtime processing stays on-device.
Do not use identifiable patient audio or confidential clinical audio for prototype
testing without organizational authorization for that specific workflow.

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

Memory checks: configure a separate Debug build with `-DLUNA_SANITIZE=ON` using GCC
or Clang, then run CTest. Tests create synthetic WAVs in a temporary directory and
remove them on normal completion; they do not open a microphone or need a network.

## Known limitations and next gate

- Physical Windows enumeration, capture, playback, long-run stability and native
  MSVC execution must still be signed off using [WINDOWS_TEST_CHECKLIST.md](WINDOWS_TEST_CHECKLIST.md).
- RNNoise v0.1 is a deliberately pinned older baseline, not the newest RNNoise.
  The distributed upstream model is embedded unchanged; no proprietary model is used.
- No acoustic echo cancellation, target-speaker isolation, background-voice removal,
  auto noise classification, virtual microphone or meeting-app integration.
- Speech quality and artifact rates have not been established on a representative
  speech/noise dataset. Noise energy reduction alone does not measure intelligibility.
- Different capture/playback clocks, USB reconnection, suspend/resume, exclusive
  devices and Bluetooth need hardware testing. Restart after a device failure.
- The native GUI has a fixed compact layout with startup DPI scaling; per-monitor
  DPI changes and full screen-reader accessibility are not yet validated.
- The app is unsigned and has no installer. No benchmark here establishes typical
  consumer-PC CPU usage or Krisp-equivalent performance.

[V2_ROADMAP.md](V2_ROADMAP.md) is a proposal gated on completing V1 hardware validation.

## Upstream sources

- [RNNoise official source](https://github.com/xiph/rnnoise), pinned tag v0.1,
  commit `cdf196b1e9de2f8ff1003328ebf9a4316477429d`.
- [miniaudio manual](https://miniaud.io/docs/manual/index.html), vendored release
  0.11.23. File hashes are in `third_party/MANIFEST.json`.
- Licenses and attributions: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
