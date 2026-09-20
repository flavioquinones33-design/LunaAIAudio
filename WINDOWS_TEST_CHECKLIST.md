# Windows hardware acceptance — not yet completed

Use this checklist on Windows 10/11 x64 with a wired microphone/headset. Use only
synthetic or authorized nonconfidential test audio. Never use patient conversations.
Record the Windows version, CPU, RAM, microphone/output model, driver version,
native rates and build compiler. These fields are currently **not supplied**.

## Reproducible steps

1. Build Release using README commands. Run CTest; every test must pass.
2. Run `luna-cli devices`. Verify actual microphone/output names against Windows
   Sound settings. A synthetic/null/discard endpoint does not satisfy this step.
3. Open the GUI. Confirm capture does not begin until Start, monitor is off, the
   selected microphone produces the input meter, and Stop releases it.
4. Select wired headphones. Start with a quiet playback volume; enable monitoring.
   Verify both enabled and bypass paths are audible with no sudden level jump.
   Toggle suppression and drag mix while speaking. Assess clicks, speech damage,
   noise pumping and low-level speech. The meters show output before monitor mute.
5. Run a 10-minute live trial, then a 30-minute stability trial. Record DSP mean/max,
   CPU, late frames/callbacks and any audible gaps. Counters alone cannot prove that
   playback was gap-free. Repeat at least 20 Start/Stop cycles.
6. Test each intended microphone/output, including a 44.1 kHz endpoint configuration.
   Confirm the application format stays 48 kHz mono. Record actual device formats.
7. While running, disconnect/reconnect USB, change defaults, deny/re-enable microphone
   permission, and suspend/resume. Expect a surfaced error or explicit restart
   requirement. Confirm no hang, crash or continued capture after Stop/close.
8. Run synthetic WAV processing, then authorized speech+fan/keyboard/traffic clips.
   Compare aligned original, bypass and denoised files at equal playback gain.
   Include quiet and accented speech, soft consonants and competing speech. Document
   losses as well as improvements; background voice isolation is not a V1 promise.
9. Check idle/live CPU in Task Manager and memory over the stability trial. The UI's
   one-core CPU scale differs from Task Manager's whole-machine percentage.
10. With networking unavailable, repeat live and WAV tests. If desired, use the
    organization's approved network monitor to verify no application network traffic.
    Check that live runs created no audio files. WAV output must require an explicit
    file-processing command and a new destination.

## Measuring physical latency

Use a separate local measurement setup with an impulse/click reference and a wired
loopback through the capture/output path. Capture a reference and returned signal
on two synchronized measurement channels in an explicitly authorized test recorder.
Calculate the sample offset by cross-correlation, convert by the measurement sample
rate, and report the median and p95 over repeated impulses. Record the interface,
driver mode, native rates and any baseline measurement-system delay. Avoid acoustic
feedback and do not turn this into a covert microphone recording feature.

Measure bypass and denoised paths separately. Compare with the documented 20 ms
software delay plus backend/driver/hardware costs. A timer around RNNoise cannot
replace this measurement.

## Proposed acceptance thresholds (targets, not achieved results)

| Check | Initial target |
|---|---|
| Automated build/tests | All pass on native Windows Release |
| DSP mean and tail | Mean < 2 ms; max < 10 ms per 480-sample frame during an otherwise idle trial |
| CPU | < 10% of one core for the whole live process on the selected baseline PC |
| Monitoring | No audible sustained glitches or accumulating delay in 30 minutes |
| Measured end-to-end latency | Median < 80 ms on a wired setup; investigate p95 excursions separately |
| Lifecycle | 20 Start/Stop cycles and device-failure trials without crash/hang |
| Privacy | No live audio files; no network audio; stop/close releases capture |
| Quality | No unacceptable speech damage in the agreed test set; no numeric quality claim until evaluated |

These are engineering acceptance proposals and may need adjustment once a target
PC/device set is chosen. Passing a single machine does not establish broad Windows
compatibility. **V1 is not complete until the physical tests are executed and results
are recorded.**
