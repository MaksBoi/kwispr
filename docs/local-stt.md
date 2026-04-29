# Local STT roadmap

Kwispr includes two local STT server entry points:

- `kwispr-local-stt-server.py` remains a tiny Python stub for wiring smoke tests.
- `rust-local-stt/` is the real inference runtime scaffold. It exposes the same OpenAI-compatible endpoint, resolves models from `models/local-stt-catalog.json`, loads Handy-compatible artifacts from `$KWISPR_MODEL_DIR` or `~/.local/share/kwispr/models`, dispatches GigaAM / Parakeet / Whisper by `engine_type`, and caches loaded engines in-process.

Start the legacy stub server:

```bash
./kwispr-local-stt-server.py --host 127.0.0.1 --port 9000
```

Health check:

```bash
curl http://127.0.0.1:9000/health
```

Stub transcription request:

```bash
printf 'dummy audio bytes' >/tmp/kwispr-dummy.wav
curl -sS http://127.0.0.1:9000/v1/audio/transcriptions \
  -F model=gigaam-v3-e2e-ctc \
  -F response_format=json \
  -F language=ru \
  -F file=@/tmp/kwispr-dummy.wav
```

Kwispr can talk to the OpenAI-compatible local transcription endpoint through:

```env
KWISPR_BACKEND=openai-transcriptions
KWISPR_API_URL=http://127.0.0.1:9000/v1/audio/transcriptions
KWISPR_MODEL=gigaam-v3-e2e-ctc
KWISPR_API_KEY=
```

The repository now includes a machine-readable local model catalog at:

```text
models/local-stt-catalog.json
```

Use `kwispr-models.py` to list, download, and verify catalog artifacts. The Rust runtime uses these installed artifacts for local inference.

## Downloading models

```bash
# Show install status for all catalog entries
./kwispr-models.py list

# Download the Russian GigaAM model into ~/.local/share/kwispr/models
./kwispr-models.py download gigaam-v3-e2e-ctc

# Verify the installed model
./kwispr-models.py verify gigaam-v3-e2e-ctc
```

Set `KWISPR_MODEL_DIR=/path/to/models` or pass `--model-dir /path/to/models` to override the install directory. The helper verifies artifact SHA256 before installation, extracts `.tar.gz` directory artifacts safely, places single-file artifacts directly, and skips redownloading already-valid models.

## Rust runtime

Build and run when Rust/Cargo and native transcribe-rs dependencies are available:

```bash
cd rust-local-stt
cargo build --release
KWISPR_MODEL_DIR=~/.local/share/kwispr/models \
  ./target/release/kwispr-local-stt --host 127.0.0.1 --port 9000 \
  --catalog ../models/local-stt-catalog.json
```

The endpoint is OpenAI-compatible:

```bash
curl -sS http://127.0.0.1:9000/v1/audio/transcriptions \
  -F model=gigaam-v3-e2e-ctc \
  -F response_format=json \
  -F language=ru \
  -F file=@sample.wav
```

Expected success response:

```json
{"text":"..."}
```

Clear HTTP errors are returned as `{"error":"..."}`:

- `400` for malformed multipart, missing fields, unsupported response format, or invalid WAV input
- `404` for unknown catalog model ids
- `422` for model resolution, load, or runtime transcription failures
- `500` for unexpected server failures

Loaded engines are cached by model id for the life of the process, so repeated requests to the same model do not reload the model.

## Optional VAD preprocessing

The Rust runtime has an optional lightweight VAD-style preprocessing hook before local inference. It is disabled by default so existing local and cloud/OpenRouter/OpenAI behavior stays unchanged. Enable it only for the local runtime:

```bash
KWISPR_VAD_ENABLED=1 \
KWISPR_VAD_THRESHOLD=0.01 \
KWISPR_MODEL_DIR=~/.local/share/kwispr/models \
  ./target/release/kwispr-local-stt --host 127.0.0.1 --port 9000 \
  --catalog ../models/local-stt-catalog.json
```

Equivalent CLI flags are available: `--vad-enabled true`, `--vad-threshold`, `--vad-frame-ms`, `--vad-min-speech-ms`, and `--vad-padding-ms`. `/health` reports the active VAD config.

Current behavior is a safe energy/RMS gate rather than full Silero ONNX VAD: audio is split into frames, frames above `KWISPR_VAD_THRESHOLD` are treated as voiced, leading/trailing silence is trimmed with padding, and clips with less than `KWISPR_VAD_MIN_SPEECH_MS` of voiced frames return an empty transcript without loading/running a model. This handles synthetic silence and short noise safely, reduces junk audio sent to STT, and helps avoid hallucinated transcripts from no-voice clips.

Why VAD matters: it trims leading/trailing silence, detects voice vs. non-voice audio, feeds less junk into STT, lowers latency for padded recordings, and reduces hallucinations on silence/noise. The config is intentionally compatible with a future Handy/Silero-style VAD provider, but Silero ONNX model integration is not implemented yet; follow-up: https://github.com/blockedby/kwispr/issues/8.

## Model recommendations

| Dictation need | Recommended model | Model id | Notes |
|---|---|---|---|
| Russian | GigaAM v3 | `gigaam-v3-e2e-ctc` | Best default for Russian-only dictation and the smallest current artifact. |
| Mixed Russian/English | Parakeet V3 or Whisper Large v3 Turbo | `parakeet-tdt-0.6b-v3` or `whisper-large-v3-turbo` | Parakeet covers ru/en and many European languages; Whisper Turbo is the broad fallback. |
| English low latency | Parakeet now; Moonshine-class models later | `parakeet-tdt-0.6b-v3` | The catalog does not include Moonshine yet, so mention it only as a future option. |

No cloud key is required for local mode. Keep `KWISPR_API_KEY=` empty when `KWISPR_API_URL` points at `127.0.0.1`. Cloud backends still require their usual OpenAI/OpenRouter key.

## Troubleshooting local mode

| Symptom | Likely cause | Fix |
|---|---|---|
| `curl: (7) Failed to connect` | Local server is not running or the port does not match `.env` | Start the Python stub or Rust runtime, then check `/health`. |
| `[stub transcript]` | You are using `kwispr-local-stt-server.py` | Use the Rust runtime for actual model inference. |
| `unknown model` | `KWISPR_MODEL` is not an id in `models/local-stt-catalog.json` | Run `./kwispr-models.py list` and copy an exact model id. |
| `model ... is not installed` | The catalog artifact has not been downloaded or `KWISPR_MODEL_DIR` points elsewhere | Run `./kwispr-models.py download <model-id>` and verify the same model dir is used by the runtime. |
| `unsupported engine_type` / model load failure | Native transcribe-rs dependency or engine support is unavailable for that model on this machine | Rebuild `rust-local-stt`, try another catalog model, or fall back to cloud mode. |
| Unsupported or incorrect language | Selected model does not support that language, or does not honor `language` selection | Use GigaAM for Russian; use Parakeet/Whisper Turbo for mixed ru/en; leave language empty for autodetect where supported. |
| Need GPU/CPU fallback control | The current runtime does not expose a GPU/CPU switch | Use the default native path for now; if unavailable, use a cloud backend until runtime selection is added. |

## Initial catalog slice

| Model | Engine | Best for | Artifact |
|---|---|---|---|
| GigaAM v3 | `gigaam` | Russian dictation | directory archive |
| Parakeet V3 | `parakeet` | mixed ru/en/uk and European languages | directory archive |
| Whisper Large v3 Turbo | `whisper` | general fallback | single GGML file |

The catalog records:

- stable model id
- display name and description
- engine type
- artifact URL
- SHA256 checksum
- approximate size
- archive/file layout
- supported languages
- intended use cases

## Why Handy-compatible metadata?

Handy already demonstrates a working local STT architecture with downloadable models, `transcribe-rs`, Whisper/ONNX engines, and VAD. Kwispr should stay small and script-first, but future local backends can reuse the same model metadata shape.

## Future slices

1. local OpenAI-compatible server skeleton
2. real `transcribe-rs` inference runtime
3. docs and integration polish
4. optional VAD preprocessing
5. future catalog expansion, such as Moonshine-class English low-latency models when suitable artifacts are selected

VAD now has an optional local-runtime preprocessing hook. The first implementation is a conservative energy/RMS gate; full Silero ONNX integration remains tracked in https://github.com/blockedby/kwispr/issues/8.
