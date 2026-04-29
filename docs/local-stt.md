# Local STT roadmap

Kwispr can already talk to an OpenAI-compatible local transcription endpoint through:

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

Use `kwispr-models.py` to list, download, and verify catalog artifacts. Kwispr does not run these models yet; the installed artifacts are intended for future local `transcribe-rs` server slices.

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
