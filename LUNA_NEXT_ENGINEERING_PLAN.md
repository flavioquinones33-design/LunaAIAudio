# Luna Audio AI — Luna Next Engineering Plan

## Purpose

This document defines the controlled evolution of Luna Audio AI from its current Windows desktop noise-suppression prototype into a modular real-time speech-intelligence platform. The program prioritizes stability, reversibility, local processing, measurable performance, and preservation of the current working baseline.

## Current baseline

The current application already provides several strong foundations:

- Native Windows desktop application.
- C++17 / CMake build.
- WASAPI capture and monitoring through miniaudio.
- 48 kHz mono float internal interface.
- RNNoise behind a `NoiseSuppressionEngine` abstraction.
- Fixed-size, bounded real-time frame processing.
- 10 ms framing and an approximately 20 ms application pipeline delay in the current RNNoise path.
- Lock-free control/metrics design for the audio callback.
- Offline WAV processing through the same `AudioProcessor` used for live processing.
- Tests for frame adaptation, WAV handling, latency alignment, and core processing.

The current architecture is therefore suitable for incremental extension. A full rewrite is not justified.

## Problem definition

Luna must distinguish three different engineering problems:

1. **Noise suppression** — fans, keyboard, HVAC, traffic and stationary/non-speech noise.
2. **Speech interference suppression** — TV, music, podcasts and recorded speech.
3. **Competing-speaker suppression** — removing or reducing other live speakers while preserving the intended user.

RNNoise addresses the first category but is not sufficient for the third. Luna Next must treat target-speaker preservation as a separate problem.

## Safety principle

`main` remains the stable reference branch.

Development of Luna Next must occur in isolated branches and must preserve a usable audio path at all times.

Preferred runtime modes:

- `LEGACY` — current validated RNNoise pipeline.
- `NEXT` — experimental Luna Next pipeline.
- `PASSTHROUGH` — raw/bounded audio path used as a final fallback.

Failure of an AI component must never result in loss of microphone audio.

## Target architecture

```text
Physical microphone
        |
        v
Capture / WASAPI
        |
        v
Audio normalization + bounded framing
        |
        v
DSP pre-stage
        |
        v
Speech Intelligence Pipeline
  - VAD
  - Noise suppression
  - Interference detection
  - Multi-speaker detection
  - Speaker representation
  - Target-speaker decision
  - Voice isolation / extraction
        |
        v
Quality / observability layer
        |
        +--> File / benchmark output
        |
        +--> Monitoring output
        |
        +--> Future Luna Virtual Microphone
```

## Real-time invariants

The audio callback must continue to avoid:

- unbounded allocation;
- file I/O;
- blocking network calls;
- UI calls;
- logging with locks;
- unbounded queues;
- model initialization;
- uncontrolled retry loops.

Any model that cannot meet the callback time budget must run behind a bounded worker mechanism with explicit latency/dropout telemetry.

## Work packages

### WP0 — Baseline Safety and Reproducibility

**Goal:** freeze and document the current known-good state before changing architecture.

Tasks:

- Treat current `main` commit as the reference baseline.
- Verify clean native Windows build from repository source.
- Run existing tests.
- Run current Windows acceptance checklist.
- Record CPU, RAM, DSP time, callback time, overruns and observed stability.
- Verify generated executable hashes where appropriate.
- Record compiler/toolchain versions.
- Record third-party dependency versions and licenses.
- Confirm that the current build can be reproduced on a clean environment.

**Acceptance gate:** no Luna Next code changes until the current application builds, tests and executes reproducibly.

### WP1 — Modular Runtime Architecture

**Goal:** prepare the codebase for multiple processing pipelines without changing current audio behavior.

Tasks:

- Introduce a processing-pipeline abstraction above individual suppression engines.
- Preserve the existing `NoiseSuppressionEngine` contract.
- Add explicit pipeline mode selection internally (`LEGACY`, `NEXT`, `PASSTHROUGH`).
- Add feature flags for future modules.
- Keep control changes lock-free where required.
- Add regression tests proving that `LEGACY` remains behaviorally equivalent.

**Acceptance gate:** legacy output and latency behavior remain within defined regression tolerance.

### WP2 — Benchmark and Observability

**Goal:** make all later AI decisions evidence-based.

Create a controlled benchmark set covering:

- clean speech;
- fan;
- keyboard;
- traffic;
- music;
- television;
- podcast;
- distant competing speaker;
- near competing speaker;
- simultaneous speech;
- mixed speech + music + noise;
- silence.

Record at minimum:

- model/pipeline version;
- hardware identifier class;
- average and P95 DSP time;
- callback timing;
- CPU and memory;
- dropped/late frames;
- clipping/invalid samples;
- target-speech preservation;
- interfering-speaker leakage;
- perceptual evaluation result.

Do not claim target-speaker performance from noise-energy metrics alone.

### WP3 — Interference Intelligence

**Goal:** detect conditions before attempting aggressive suppression.

Candidate outputs:

- speech present;
- probable multiple speakers;
- probable music/TV/recorded speech;
- interference confidence;
- uncertainty state.

Detection must initially be advisory and must not automatically alter the audio path until validated.

### WP4 — Speaker Intelligence

**Goal:** determine whether detected speech belongs to the intended user.

Research and prototype locally:

- speaker embeddings;
- temporal speaker continuity;
- confidence thresholds;
- optional user enrollment;
- false acceptance / false rejection measurement.

Any voice profile must remain local by default and should be protected at rest.

### WP5 — Voice Isolation

Progress through three generations:

#### Isolation V1

Conservative heuristics and existing DSP/AI components.

#### Isolation V2

Neural speech-separation comparison using offline benchmark first.

#### Isolation V3

Target-speaker extraction conditioned on a locally stored speaker representation.

Candidate models must be evaluated on:

- license;
- Windows compatibility;
- CPU cost;
- memory cost;
- inference latency;
- ONNX or equivalent deployment path;
- quantization feasibility;
- target-speech distortion;
- unwanted-speaker attenuation.

No model is selected solely because it is popular.

### WP6 — Virtual Audio Device

This work starts only after the core real-time pipeline is stable.

Evaluate:

- supported user-mode approaches;
- signed virtual audio driver approaches;
- PCM transport design;
- bounded buffering;
- device-clock drift;
- sample-rate mismatch;
- endpoint reconnects;
- crash recovery;
- installer/uninstaller behavior;
- Windows update compatibility.

The virtual microphone must be removable without affecting physical audio devices.

### WP7 — Application Integration

Validate the future virtual endpoint with:

- Teams;
- Zoom;
- browser WebRTC applications;
- softphones;
- VoIP clients.

Test application-side AGC/noise suppression interactions and device switching.

### WP8 — Reliability, Security and Privacy

Requirements:

- local-first audio processing;
- no automatic audio recording;
- no cloud audio upload by default;
- no conversational content in telemetry;
- bounded logs;
- crash isolation;
- clean device recovery;
- dependency inventory;
- reproducible builds;
- explicit rollback path.

### WP9 — Release Candidate

Required before stable release:

- signed/reproducible Windows build strategy;
- installer/uninstaller validation;
- compatibility matrix;
- regression suite;
- documented privacy behavior;
- documented third-party notices;
- release notes;
- rollback procedure.

## Latency strategy

The current architecture already uses a 48 kHz interface and bounded frames. Luna Next should preserve a strict latency budget.

Initial engineering target for added application latency:

- desirable: 20–40 ms;
- maximum target for interactive speech: under 50 ms where feasible.

These are engineering targets, not measured current totals. Physical end-to-end latency must be measured externally.

## Experiment tracking

Every model comparison should record:

- Luna version / commit;
- pipeline mode;
- model name and exact artifact version;
- parameters;
- hardware class;
- input fixture identifier;
- output fixture identifier;
- timing metrics;
- CPU / memory;
- quality metrics;
- listening-test result;
- known failure cases.

Historical experiment results must never be overwritten.

## Risk register

| Risk | Impact | Detection | Mitigation |
|---|---|---|---|
| Target voice suppressed | Critical | benchmark + listening tests | confidence gating + legacy fallback |
| Excessive latency | High | timing telemetry | reject model / worker queue / quantization |
| Audio dropout | High | frame/callback counters | bounded queues + passthrough fallback |
| High CPU usage | High | process metrics | model selection + optimization |
| Windows endpoint failure | Critical | device state monitoring | isolate virtual-device layer |
| Buffer growth | Critical | queue depth telemetry | bounded buffers only |
| Model initialization on callback | High | code review/tests | pre-warm on control thread |
| Privacy leak | Critical | architecture review | local-first, no audio telemetry |
| License incompatibility | High | dependency review | block integration until cleared |
| Benchmark overfitting | Medium | held-out scenarios | separate validation fixtures |
| Dependency abandonment | Medium | dependency audit | pin versions + replacement plan |
| Irreproducible build | High | clean-build validation | toolchain and artifact pinning |

## Decision gates

No work package is accepted because audio merely "sounds better".

Each gate must define measurable thresholds appropriate to the hardware target and dataset. Thresholds that are not yet supported by evidence must remain explicitly undecided until benchmark data exists.

## Immediate execution order

1. Complete WP0 evidence collection on Windows.
2. Do not modify `main`.
3. Develop architecture changes only in `feature/luna-next-architecture` or narrower child branches.
4. Build WP1 without changing the current audible behavior.
5. Add benchmark/observability before integrating any new speaker-isolation model.
6. Compare models offline before allowing them into the live callback path.
7. Defer the virtual microphone until core isolation is stable and measurable.

## Definition of success

Luna Next succeeds when it can improve target-speaker communication quality while preserving the following properties:

- stable real-time operation;
- bounded latency and memory;
- deterministic fallback;
- privacy by default;
- reproducible engineering evidence;
- no dependency on unauthorized proprietary technology;
- no regression of the current known-good audio path.
