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

This catalog is metadata only. Kwispr does not download or run these models yet. It is intended to support future stacked PRs for a downloader and local `transcribe-rs` server.

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

1. model downloader and checksum verification
2. local OpenAI-compatible server skeleton
3. real `transcribe-rs` inference runtime
4. docs and integration polish
5. optional VAD preprocessing

VAD is planned because it can trim silence, detect voice/non-voice audio, avoid feeding junk into STT, and reduce hallucinated transcripts.
