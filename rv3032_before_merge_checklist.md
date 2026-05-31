# RV3032-C7 — What To Do Before Merge

Branch: `hardening/rv3032-industry-readiness`

## Merge gate

This branch is code-ready for review, but merge only after the final release/documentation polish and a real CI run.

## 1. Final documentation polish

- Reconcile README hardware specs with the RV3032-C7 datasheet/app manual:
  - supply/timekeeping voltage,
  - current numbers,
  - accuracy claim,
  - temperature range and limitations.
- Fix suspicious README quickstart typo if present:
  - `RV3032::RV3032::parseBuildTime(now)` should likely be checked/corrected.
- Ensure README, hardware matrix, and reports use only real CLI commands:
  - `drv`, not `state`,
  - `statusf` / `validity`, not generic `flags`,
  - `set YYYY MM DD HH MM SS`, not `settime`,
  - `stress N`, not `stress_time` unless implemented.
- Make wording consistent everywhere:
  - allowed: “production-oriented hardening branch”,
  - not allowed yet: “field-proven industry-grade”.

## 2. Version and changelog decision

- Decide SemVer bump before release.
- Do not silently ship this as unchanged `1.5.0` if public users may be affected.
- Copy/move deletion and changed probe/recover behavior are breaking-ish; conservative option is a major bump.
- Update:
  - `library.json`,
  - generated `Version.h`,
  - `idf_component.yml` if generated/synced,
  - `CHANGELOG.md`,
  - final report release notes.

## 3. Run local checks

Run and record exact results:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio pkg pack
```

After package inspection, remove generated package artifacts from the worktree.

## 4. Push and verify CI

- Push the branch.
- Verify GitHub Actions actually passes on the pushed branch or PR.
- Do not claim ESP-IDF CI passed until a real workflow run is observed.
- Confirm CI covers:
  - guard scripts,
  - native tests,
  - Arduino ESP32-S2/S3 builds,
  - package validation,
  - ESP-IDF example builds for ESP32-S2/S3.

## 5. Final merge report

Update or add a short final pre-merge note under `docs/` with:

- commit hash,
- local checks run,
- CI link/status,
- version decision,
- docs/spec corrections made,
- remaining hardware-validation limitations.

## Merge verdict

Merge after:

1. docs/spec polish is complete,
2. version/changelog are intentionally handled,
3. local checks pass,
4. GitHub Actions passes,
5. final report clearly says hardware validation is still pending.

Hardware validation can be a separate bench pass after merge, but do not make field/industry-grade release claims until it is executed and recorded.
