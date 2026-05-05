# Agent instructions for Kwispr

## Default branch and workflow

- Treat `openrouter-backend` as the active integration branch for this fork unless the user says otherwise.
- Use dedicated git worktrees for parallel agent work; do not edit the main checkout from multiple agents at once.
- Keep changes small, scoped to the assigned issue, and commit only after verification passes.

## Prefer Podman for builds and tests

This project intentionally carries Podman build/test wrappers so agents do not depend on the host having every KDE/Rust/Vulkan dependency installed.

### KDE Whisper

For KDE/Qt/CMake changes under `kde-whisper/`, prefer:

```bash
./kde-whisper/scripts/podman-test.sh
```

For targeted KDE tests:

```bash
./kde-whisper/scripts/podman-test.sh -R <CTestName>
```

Do not assume the host has all Qt/KF6/CMake/Vulkan pieces even if a local `kde-whisper/build/` exists.

### Local STT Rust runtime

For `rust-local-stt/` changes, prefer the Podman builder because the host may lack Rust/CMake/Vulkan headers:

```bash
./rust-local-stt/build-in-podman.sh
```

For Rust tests, use the same builder image instead of plain host `cargo test`:

```bash
podman build -t kwispr-local-stt-builder -f rust-local-stt/Containerfile rust-local-stt
podman run --rm \
  -v "$PWD":/work:Z \
  -w /work/rust-local-stt \
  kwispr-local-stt-builder \
  cargo test
```

If a host `cargo test` fails with missing `Vulkan_INCLUDE_DIR` or similar system dependency errors, do not treat that as a code failure until the Podman path has been tried.

## Existing reliable CLI path

- `kwispr.sh` is the reliable dictation toggle path; do not break it while adding KDE/native features.
- Keep default behavior unchanged unless an opt-in environment variable explicitly enables new behavior.
- Contract tests live under `tests/`, especially `tests/test_kwispr_sh_contract.py`.

## Verification expectations

- For shell/CLI changes: run relevant Python unittest(s), e.g.

```bash
python3 -m unittest tests/test_kwispr_sh_contract.py
python3 -m unittest discover
```

- For KDE changes: run the KDE Podman test wrapper.
- For Rust local STT changes: run Rust build/tests in Podman.
- Record exact commands and results in the final worker summary.
