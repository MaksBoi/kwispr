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

VAD is planned because it can trim silence, detect voice/non-voice audio, avoid feeding junk into STT, and reduce hallucinated transcripts.
