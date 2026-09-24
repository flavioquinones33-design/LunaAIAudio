# V2 proposal — gated on V1 hardware acceptance

Do not expand the product until native Windows builds, physical capture, monitoring,
latency, quality and stability pass the V1 checklist. No V2 item below is implemented.

| Order | Work | Evidence required before release |
|---|---|---|
| 1 | Windows virtual microphone | Evaluate supported user-mode integration versus a signed virtual audio driver. Define PCM transport, bounded buffering, device clock drift, crash recovery and opt-in installation. Preserve user device selection and uninstall cleanly. |
| 2 | Zoom / Google Meet / Teams | Test the virtual endpoint in current desktop/browser clients, their permissions, sample-rate negotiation, app noise reduction/AGC interaction and device switching. Verify reconnects and multi-app use. |
| 3 | DeepFilterNet comparison | An optional external C API adapter and Linux WAV smoke test exist. Review exact code/model licenses and pinned artifacts; validate the Windows DLL and real-time path. Compare CPU, memory, measured latency and speech quality against RNNoise on the same authorized corpus. Ship no silent model download. |
| 4 | Speaker/background-voice suppression | Treat this as a separate target-speaker enhancement problem. Define intended behavior and consent before any enrollment/voiceprint approach. Measure target speech distortion and unwanted speaker leakage; RNNoise alone is insufficient. |
| 5 | Automatic noise classification | Optional local classifier with categories, uncertainty and a manual override. Evaluate errors before letting classification alter processing. Do not infer clinical information or user identity. |
| 6 | Installer and signed Windows build | Reproducible native CI, dependency inventory/hashes, publisher certificate, timestamped signing, install/uninstall/update testing, and signed-driver requirements if applicable. No telemetry or cloud-audio path introduced. |

For comparative quality evaluation, combine blinded listening with intelligibility
and perceptual metrics appropriate to the dataset. Report confidence intervals,
failure cases and test-set limits. Never turn a synthetic-noise energy measurement
into a claim of Krisp-equivalent performance or target-speaker isolation.
