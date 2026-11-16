# 03: ESP-IDF CI, package and consumer gates, closeout and the remote-CI checkpoint

**Status: NOT STARTED.** Prerequisite: 01 and 02 are DONE and pass on current
`main`. This last prompt is the only one that dispatches and waits for remote
CI.

## Outcome and scope

This prompt:

- adds the in-tree native build and the packaged-consumer native build to
  existing CI;
- extends the package and consumer rules to the IDF component and example;
- advertises `espidf`;
- finishes the docs;
- marks this promptset executed;
- proves the exact final commit on remote CI.

Authorized:

- `.github/workflows/ci.yml`:
  - the new jobs `esp-idf-basic` and `esp-idf-package`;
  - in `validate-library` (`:80-120`), one static step and one package
    artifact upload.
  Keep the triggers at `:3-8`, and add no `workflow_dispatch` (`AGENTS.md:19`).
- `library.json:5-19,39-41,52-75`: keywords, frameworks, export.
- `idf_component.yml` `files.exclude`.
- `tools/check_package.py`: required package files, forbidden IDF output, and
  the packaged-manifest check.
- `tools/check_idf_example_contract.py` and its test: CI tokens.
- `README.md:1-14,116-119,507-517,545-627`, `docs/README.md`,
  `docs/IDF_PORT.md`, `docs/VERIFICATION.md:8-60`.
- `AGENTS.md`: target, planning note, repository model, release steps.
- `CHANGELOG.md` `[Unreleased]`, and the status lines of `docs/prompts/*.md`.

Forbidden:

- core changes beyond a CI-proven build fix;
- weakening any check;
- deleting `docs/prompts/` or its source-check exception;
- any version, tag or release change.

## Audit before editing

- **CI.** `.github/workflows/ci.yml`. Its action style is tags, such as
  `actions/checkout@v7`. Jobs: `build` (`:11-45`), `native-tests`
  (`:47-78`), `validate-library` (`:80-120`). The last of these packs
  `dist/RV3032-C7.tar.gz` and validates it (`:116-120`).
- **Gates.** `docs/VERIFICATION.md:8-45`, plus `tools/check_package.py` and
  `tools/check_idf_example_contract.py` as left by 01 and 02.
- **Action refs** **[verify]** with
  `gh api repos/<owner>/<repo>/git/ref/tags/<tag>` (or `.../ref/heads/<name>`
  for a branch). Checked on 2026-11-16:
  - `espressif/esp-idf-ci-action@v1.2.0` is a tag (commit `e6f5c742`). Its
    `v1` is a branch, not a tag; its head `9d38657f` is one commit ahead of
    v1.2.0 and changes only `README.md`. Name the tag `@v1.2.0`, which matches
    this workflow's tag style and cannot move. Siblings use `@v1` or the SHA
    `9d38657f`.
  - `actions/upload-artifact@v4` (`ea165f8d`) and
    `actions/download-artifact@v4` (`d3f86a10`) are tags.
- **Siblings** (read-only; record revisions).
  - INA228 `.github/workflows/ci.yml:95-124` (`esp-idf-basic`).
  - MCP45HVX1 `idf_component.yml:13-34` and TCA9548A
    `idf_component.yml:17-29`: excludes.
- `gh run list --workflow ci.yml --branch main --limit 5` is context only, not
  evidence.

## Implement

1. **CI.**
   - **`esp-idf-basic`**, shaped like INA228 `ci.yml:95-124`:
     - `ubuntu-latest`, `fail-fast: false`, matrix `[esp32s3, esp32s2]`;
     - checkout, then `espressif/esp-idf-ci-action@v1.2.0` with
       `esp_idf_version: v6.0.1`, `target: ${{ matrix.target }}`,
       `path: examples/esp_idf/basic` and
       `command: idf.py set-target ${{ matrix.target }} build`;
     - a log upload with `actions/upload-artifact@v4`, `if: always()` and
       `if-no-files-found: ignore`.
   - **`validate-library`.** Run `python tools/check_idf_example_contract.py`
     before the package step. After
     `python tools/check_package.py package dist/RV3032-C7.tar.gz`, upload
     `dist/RV3032-C7.tar.gz` with `actions/upload-artifact@v4`, a fixed name
     and `if-no-files-found: error`.
   - **`esp-idf-package`** (`needs: validate-library`, `ubuntu-latest`,
     `fail-fast: false`, matrix `[esp32s3, esp32s2]`) runs without checkout,
     so only package files exist:
     - download the artifact with `actions/download-artifact@v4` into
       `$GITHUB_WORKSPACE`;
     - `mkdir -p "$GITHUB_WORKSPACE/pkg/RV3032"`, then
       `tar -xzf <archive> -C "$GITHUB_WORKSPACE/pkg/RV3032"`. The IDF action
       mounts only the workspace, so the package must lie under it. The archive
       is flat, and the folder is deliberately not named `RV3032-C7`;
     - fail, never skip, when
       `pkg/RV3032/examples/esp_idf/basic/CMakeLists.txt` or the root
       `CMakeLists.txt` is missing;
     - `espressif/esp-idf-ci-action@v1.2.0` with `esp_idf_version: v6.0.1`,
       `target: ${{ matrix.target }}`,
       `path: pkg/RV3032/examples/esp_idf/basic` and
       `command: idf.py set-target ${{ matrix.target }} build`;
     - the identity header reads `unknown` here, because there is no Git.
   - The checker requires the tokens of both jobs and of the upload, with
     mutation cases in `tools/test_check_idf_example_contract.py`.
2. **Package.**
   - `REQUIRED_PACKAGE_FILES` (`check_package.py:27-39`) gains the root
     `CMakeLists.txt`, `idf_component.yml`, the engine and every
     `examples/esp_idf/basic` source, CMake, sdkconfig and README file.
   - `forbidden_package_path()` rejects IDF build output and lock files under
     `examples/`. It keeps `idf_component.yml.orig` and `docs/prompts/**`.
   - Package mode re-runs 01's check on the packaged manifest against the
     packaged `library.json`.
   - The `library.json` export and the manifest `files.exclude` stay in step,
     and a test catches drift.
   Add mutation tests for each rule.
3. **`library.json`.** Add `"espidf"` to `frameworks` and an `esp-idf` keyword.
   State in `README.md` and `docs/IDF_PORT.md`: "`library.json` lists `espidf`
   because the package ships the ESP-IDF component and native example;
   PlatformIO `framework = espidf` builds are not tested (only `idf.py` 6.0.1
   for esp32s3/esp32s2)."
4. **Consumers (local, before the checkpoint; same commands as CI).**
   - Pack to `C:\esp\build\rv3032-pkg\RV3032-C7.tar.gz`, then extract it with
     `tar -xzf` into `C:\esp\build\rv3032-pkg\RV3032`. Never use `%TEMP%`.
   - Build the extracted example for both targets with
     `-B C:\esp\build\rv3032-pkg-<target>-6.0.1`.
   - Build a throwaway consumer outside the repo, `C:\esp\build\rv3032-consumer`.
     Its `main/idf_component.yml` declares
     `RV3032: {path: C:/esp/build/rv3032-pkg/RV3032}`, which exercises the root
     CMake and manifest **[verify]**. Record the resolved component name. If it
     cannot resolve, report BLOCKED with the exact error; never add
     `override_path`.
   - Delete the scratch trees after you record the results.
5. **Docs.**
   - `README.md:1-14`: native ESP-IDF alongside Arduino.
   - `README.md:116-119`: both examples.
   - `README.md:514`: the support boundaries, with the `espidf` statement.
   - `README.md:545-563`: separate Wire and IDF adapter sections. Both make one
     attempt per callback.
   - `README.md:582-627`: layout and verification.
   - `docs/VERIFICATION.md`: the commands, the IDF and packaged-consumer gates,
     the hardware surfaces, and the manifest text.
   - `docs/IDF_PORT.md`: the final state.
   - `docs/README.md`: relabel the link as the executed historical promptset.
   - `AGENTS.md`: the shipped target replaces the planning note, which may
     point to `docs/prompts/` as executed historical requirements. The release
     steps keep `idf_component.yml`'s version equal to `library.json`'s.
   - CHANGELOG: replace the planning bullet with user-visible Added, Changed
     and Fixed entries, including the shared engine and "Arduino behavior
     unchanged".
6. **Mark the set executed.** Keep `docs/prompts/`.
   - Set the README header to
     `**Status: EXECUTED (historical requirements, not open work); hardware NOT RUN.**`
     and every row to DONE.
   - Keep the source-check exception and
     `test_promptset_is_source_only_and_never_packaged`; they are permanent
     since this set was written.
   - Keep `docs/prompts/**` in `REQUIRED_EXPORT_EXCLUDES` and
     `forbidden_package_path()`.
7. **Remote-CI checkpoint**, once, on the exact final commit:
   1. Run the full Verify list, then commit, push and sync.
   2. `gh run list --workflow ci.yml --branch main --commit <sha> --json databaseId,headSha,status,conclusion`.
   3. `gh run watch <id> --exit-status`, then record each job from
      `gh run view <id> --json jobs`: `build` four times, `native-tests`,
      `validate-library`, `esp-idf-basic` twice and `esp-idf-package` twice.
   4. If any job fails, fix the cause without weakening a check, commit, push
      and repeat on the new SHA.
   5. Never cite an older run.

## Acceptance

- CI is green on the exact final SHA, with every job named, including both
  `esp-idf-package` legs.
- The package holds the IDF files and no build output. The extracted copy
  builds for both targets, in CI and locally.
- The consumer build passes, or is BLOCKED with the exact error.
- No document overstates the evidence.
- `docs/prompts/` remains, marked EXECUTED. The source check admits only its
  top-level Markdown, and packages reject it.
- Hardware stays NOT RUN.

## Verify

```powershell
# 01's full Verify list, plus:
python tools/check_idf_example_contract.py
C:\esp\idf.cmd 6.0.1 -C C:\esp\build\rv3032-pkg\RV3032\examples\esp_idf\basic -B C:\esp\build\rv3032-pkg-esp32s3-6.0.1 -D SDKCONFIG=C:\esp\build\rv3032-pkg-esp32s3-6.0.1.sdkconfig set-target esp32s3 build
C:\esp\idf.cmd 6.0.1 -C C:\esp\build\rv3032-pkg\RV3032\examples\esp_idf\basic -B C:\esp\build\rv3032-pkg-esp32s2-6.0.1 -D SDKCONFIG=C:\esp\build\rv3032-pkg-esp32s2-6.0.1.sdkconfig set-target esp32s2 build
C:\esp\idf.cmd 6.0.1 -C C:\esp\build\rv3032-consumer -B C:\esp\build\rv3032-consumer-esp32s3-6.0.1 -D SDKCONFIG=C:\esp\build\rv3032-consumer-esp32s3-6.0.1.sdkconfig set-target esp32s3 build
git diff --check
```

## Close out

The handoff reports the six layers separately:

- host tests;
- static/contract checks;
- native ESP-IDF build (S2/S3), in-tree;
- package/clean consumer, including the native payload build (local and CI);
- remote CI: run ID, SHA and each job's conclusion;
- physical hardware: NOT RUN.

Put the CI-verified SHA only in the handoff, so no commit refers to itself.
Remaining gates: physical HIL with an authorized fixture, console input on a
target, and PlatformIO `espidf` builds.
