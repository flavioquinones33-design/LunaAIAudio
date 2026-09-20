# Privacy

- **Audio remains local.** Captured microphone PCM is processed in this process's
  memory and sent only to the selected local audio output endpoint.
- **No cloud processing.** RNNoise inference and all format conversions run on-device.
  The model is included in the executable.
- **No telemetry containing audio.** There is no telemetry system, network client,
  audio upload, analytics service, account, update check or remote inference path.
- **Recording is disabled by default.** V1 has no live recording feature at all.
  Opening the UI does not begin capture; Start audio is required. Monitoring is also
  off by default.

## Explicit file operations

The command `luna-cli wav INPUT OUTPUT` explicitly enables creation of a new local
processed WAV from a user-selected input file. It is not a microphone recorder.
The `fixture` command writes synthetic tones/noise for testing; no person is recorded.
The app refuses to overwrite existing files. Test output is not deleted after a
successful explicit WAV conversion; the user controls its retention and deletion.

Use local, unsynced directories. The application does not implement or control
Windows cloud-folder synchronization, mapped network drives, OS diagnostics,
pagefiles, audio drivers, other applications or third-party virtual audio devices.
It does not create or use any audio network transport itself. RAM is released on
shutdown; the prototype does not promise forensic erasure from OS-managed memory.

## Metrics and identifiers

The UI reads device names from Windows and keeps selection/session state in memory.
The GUI does not persist settings or logs. CLI diagnostic output may contain device
names, scalar RMS/peak levels, frame counts, timing and error text. It contains no
audio samples, transcripts, voiceprints or speaker identification. Redirecting CLI
output to a file is an explicit user shell operation.

## Clinical and organizational restrictions

Prototype testing should use synthetic signals or authorized, nonconfidential audio.
Do not provide, process, infer from, or retain identifiable patient information or
confidential clinical audio without explicit organizational authorization for that
specific workflow and applicable privacy, security, consent and access controls.
If authorization or de-identification is uncertain, use a de-identified test asset
instead. On-device processing alone does not establish organizational or legal
compliance, and this prototype does not claim such certification.

## Verification scope

Application sources contain no networking or telemetry implementation. Build-time
dependencies are vendored, so the application never downloads a model. Review
`src/audio.cpp` for live processing, `src/wav.cpp` for the sole intentional audio-file
writer, and [VALIDATION.md](VALIDATION.md) for tests and outstanding Windows checks.
No packet-capture audit on a physical Windows host has yet been completed.
