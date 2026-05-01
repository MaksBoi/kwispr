#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${KWISPR_LOCAL_STT_BUILDER_IMAGE:-kwispr-local-stt-builder}"

podman build -t "$IMAGE" -f "$ROOT_DIR/rust-local-stt/Containerfile" "$ROOT_DIR/rust-local-stt"
podman run --rm \
  -v "$ROOT_DIR:/work:Z" \
  -w /work/rust-local-stt \
  "$IMAGE"
