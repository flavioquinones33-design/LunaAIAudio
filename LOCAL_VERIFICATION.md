# Luna Audio AI — Local Verification Workflow

## Policy

Luna Audio AI does not depend on GitHub Actions for build, test, benchmark, release acceptance or hardware validation.

GitHub is used for source control, branches, commits, issues and review. Engineering verification is executed locally on controlled developer hardware, primarily Windows 10/11 x64.

No work package is accepted merely because code was committed successfully.

## Why local verification is required

Luna depends on behavior that hosted CI cannot adequately validate:

- physical microphone enumeration and capture;
- WASAPI device behavior;
- actual playback and monitoring;
- device reconnects;
- suspend/resume;
- USB/Bluetooth edge cases;
- real CPU load on target hardware;
- physical end-to-end latency;
- future virtual microphone behavior;
- compatibility with Teams, Zoom, browsers and softphones.

Automated local tests remain mandatory where deterministic automation is possible.

## Source-control rules

- `main` remains the stable reference.
- Luna Next work is performed on `feature/luna-next-architecture` or narrower child branches.
- Changes are committed in small, reversible units.
- A failed local gate blocks promotion even if the code compiles elsewhere.
- No `.github/workflows` dependency is required for Luna Next.

## Local verification levels

### Level 0 — Source integrity

Record:

- branch;
- commit SHA;
- clean/dirty working-tree state;
- compiler version;
- CMake version;
- Windows version;
- CPU architecture;
- dependency hashes where available.

### Level 1 — Clean configure/build

From Developer PowerShell for Visual Studio 2022:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

Gate: Release build completes without errors.

### Level 2 — Deterministic automated tests

```powershell
ctest --test-dir build -C Release --output-on-failure
.\build\Release\luna-cli.exe benchmark --seconds 60
```

Gate: all expected tests pass and benchmark completes without crash or invalid output.

### Level 3 — Device enumeration and live smoke test

```powershell
.\build\Release\luna-cli.exe devices
.\build\Release\luna-cli.exe live --seconds 60
```

Use explicit device indices when required.

Gate: capture starts, remains stable for the test period and releases devices cleanly.

### Level 4 — Desktop application acceptance

Run:

```powershell
.\build\Release\LunaAudioAI.exe
```

Validate `WINDOWS_TEST_CHECKLIST.md`.

Gate: current V1 behavior remains functional on the target Windows machine.

### Level 5 — Audio regression fixtures

Create a new output folder for each run. Never overwrite historical results.

```powershell
$run = Join-Path .\out (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
New-Item -ItemType Directory -Path $run -Force | Out-Null
.\build\Release\luna-cli.exe fixture "$run\original.wav" --seconds 12
.\build\Release\luna-cli.exe wav "$run\original.wav" "$run\denoised.wav"
.\build\Release\luna-cli.exe wav "$run\original.wav" "$run\bypass.wav" --bypass
.\build\Release\luna-cli.exe wav "$run\original.wav" "$run\mix50.wav" --strength 0.5
```

Gate: file processing succeeds, output duration/alignment remain valid, and no existing input/output file is overwritten.

### Level 6 — Hardware performance baseline

Record at minimum:

- process CPU;
- process RAM;
- DSP last/mean/max;
- callback timing;
- late frames;
- late callbacks;
- invalid samples;
- unexpected device stops;
- session duration;
- pipeline mode;
- active model/engine.

Do not report physical end-to-end latency unless measured with an appropriate loopback method.

### Level 7 — Luna Next experimental acceptance

For every experimental pipeline or model, compare against the same fixtures and hardware baseline.

Required evidence:

- exact commit;
- exact model artifact/version;
- parameters;
- hardware;
- average/P95 processing timing;
- CPU/RAM;
- target-speech preservation;
- interfering-speaker leakage;
- failure cases;
- listening-test notes where relevant.

Subjective listening alone is not an acceptance gate.

## Failure policy

If a local gate fails:

1. stop promotion;
2. preserve logs and measurements;
3. identify whether failure is code, device, environment or model related;
4. revert or disable the smallest responsible change;
5. rerun the previous known-good gate;
6. document the deviation before continuing.

For runtime audio failures, the design priority is:

`NEXT -> LEGACY -> PASSTHROUGH`

A failed AI component must not leave the user without microphone audio.

## Evidence storage

Recommended repository structure for non-sensitive engineering results:

```text
validation/
  local/
    <date>-<machine-class>-<commit>/
      environment.txt
      test-results.txt
      benchmark.txt
      metrics.csv
      notes.md
```

Do not commit private conversations, confidential audio, patient data or identifiable clinical material.

Large authorized audio fixtures should remain outside Git unless their licensing, privacy and repository-size implications are explicitly accepted.

## Release gate without GitHub Actions

A release candidate is eligible only when:

1. clean local checkout builds successfully;
2. CTest passes;
3. benchmark completes;
4. Windows hardware checklist passes;
5. relevant regression audio tests pass;
6. performance remains within approved thresholds;
7. rollback is verified;
8. dependency/license review is current;
9. release artifact is generated from the recorded commit;
10. results are archived locally or in the repository without sensitive audio.

This local verification pipeline is the authoritative engineering gate for Luna Next unless the project later adopts another explicitly approved validation system.