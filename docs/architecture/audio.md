# Audio architecture

Phase 9B adds `gameengine_audio` as an optional C++ runtime module. It does not change the RHI,
the C ABI, the asset containers or the Rust workspace. A build with
`GAMEENGINE_BUILD_AUDIO=OFF` does not link the module into the runtime or editor.

## Runtime model

The mixer uses a fixed 48 kHz, stereo, `float32` format with 512-frame blocks and up to 64
voices. The only source in this phase is a deterministic sine oscillator created in memory. A
voice contains frequency, duration, gain, pan, pitch and loop state. Panning uses an equal-power
stereo law and the final samples are clamped to `[-1, 1]`.

Voice handles contain an index and generation. Slots are reserved through a fixed atomic bit mask,
and stale handles are rejected after a slot is recycled. Play, stop, stop-all and master-gain
operations are POD commands in a fixed single-producer/audio-consumer queue. The audio consumer
owns oscillator phase and active voice state.

The callback/mixer path performs no allocation and no filesystem access. Initialization reserves
all command and voice storage. Shutdown is explicit and waits for the backend worker before
releasing its device.

## Backends and fallback

Windows uses WASAPI shared mode and the default render endpoint. Linux uses ALSA through
`libasound` when the development package is available at configure time. Other platforms, Linux
builds without ALSA headers and machines without a usable output device use the stub backend.

The stub still permits deterministic offline mixer tests, but reports `available=false`. Runtime
diagnostics treat missing audio hardware as `unavailable` and exit successfully. Native backend
underruns and played frames are exposed through `AudioMetrics`; an underrun does not crash the
runtime.

## Activation and diagnostics

The normal runtime never starts the audio device and remains silent. The explicit
`--audio-smoke-test` command initializes the backend, plays a quiet 440 Hz tone for a fixed
duration, stops it and prints backend, availability, played-frame and underrun metrics. The
editor accepts the same flag after `--project`; normal editor sessions only record backend state
in the internal console.

The optional benchmark uses the null output path and fixed 1, 16 and 64 voice workloads. It
reports mixer time, active voices, processed frames, reserved memory and underruns without
versioning machine-specific results.

## Future work

File-backed audio, WAV/OGG decoding, streaming, compression, 3D spatialization, effects, music
graphs, voice prioritization and audio asset integration remain outside this phase.
