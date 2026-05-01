# KDE Whisper TDD Checklist

## Task 1 — Qt6/KF6 Podman + CMake test scaffold
- [x] Add Task 1 checklist entry.
- [x] Create scaffold tests first.
- [x] Run scaffold tests red in Podman.
- [x] Implement minimal scaffold.
- [x] Run scaffold tests green in Podman.

## Task 2 — `.env` parser/writer preserving existing Kwispr config behavior
- [x] Add Task 2 checklist entry.
- [x] Create env parser/writer tests first.
- [x] Run `EnvFileTest` red.
- [x] Implement parser/writer.
- [x] Run `EnvFileTest` green.

## Task 3 — Settings domain model and validation
- [x] Add Task 3 checklist entry.
- [x] Create settings model tests first.
- [x] Run `SettingsModelTest` red.
- [x] Implement model and validation.
- [x] Run `SettingsModelTest` green.


## Task 4 — Regression tests for existing `kwispr.sh` contract

- [x] Add Task 4 checklist entry.
- [x] Create shell contract tests first.
- [x] Run contract tests red because helpers/fakes are missing.
- [x] Implement test harness/fakes only.
- [x] Run contract tests green without changing `kwispr.sh`.

## Task 5 — Process runner for existing toggle/retry and local Rust STT server

- [x] Add Task 5 checklist entry.
- [x] Create runner tests first.
- [x] Run `ProcessRunnerTest` red.
- [x] Implement runner.
- [x] Run `ProcessRunnerTest` green.

## Task 6 — Model catalog and installer bridge
- [x] Add Task 6 checklist entry.
- [x] Create model manager tests first.
- [x] Run `ModelManagerTest` red.
- [x] Implement model manager.
- [x] Run `ModelManagerTest` green.

## Task 7 — Local server health/status client
- [x] Add Task 7 checklist entry.
- [x] Create health client tests first.
- [x] Run `LocalSttClientTest` red.
- [x] Implement health client.
- [x] Run `LocalSttClientTest` green.
