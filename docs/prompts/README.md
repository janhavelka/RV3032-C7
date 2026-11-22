# RV3032-C7 native ESP-IDF promptset

Prepared 2026-11-16 on `main` after `e0d3f1f573bc62efddf84072edf7296a4e02029c`.
Line citations refer to the tree that adds this folder. **Status: IN
PROGRESS.** The table below tracks execution. Writing the prompts changed no
library, example, package or workflow. Their preparation added one
permanent, tested source-check exception that admits this folder
(`tools/check_package.py:70-82`); packages still reject it. The SHA is an
evidence anchor, not an instruction to reset the tree. If `main` has moved,
re-read the cited sources and follow the source where it differs.

## Starting point on 2026-11-16

On 2026-11-16 the owner elected pure ESP-IDF support with a native example,
built like the owner's other libraries. The core is already framework-neutral
(`include/RV3032/Config.h:8-11,35-82`; `docs/IDF_PORT.md:7-23`). The repository
then shipped no ESP-IDF component, adapter, native example or IDF CI
(`README.md:514`, `docs/IDF_PORT.md:3-5`). It ignored and rejected a root
`idf_component.yml` (`.gitignore:54-55`, `library.json:65-66`,
`tools/check_package.py:44,170`, `docs/VERIFICATION.md:43-45`). Its only CLI was
an Arduino sketch with no framework-neutral engine
(`examples/01_basic_bringup_cli/main.cpp:16-31,2190-2369`). The unmerged branches
`origin/feature/rv3032-idf-port` (`4b97264`) and
`origin/hardening/rv3032-industry-readiness` (`ce6bcde`) hold older IDF
attempts. Read them only with `git show`. Do not merge, cherry-pick or restore
their files or APIs; build on the current core.

## Invocation

One prompt (replace `NN_FILE`):

```text
In C:\Users\Honza\Documents\Projects\RV3032-C7, execute docs/prompts/NN_FILE.md
completely. First read AGENTS.md, ..\AGENTS.md and docs/prompts/README.md and
follow them. Work on main: fetch, check status, branch and upstream,
fast-forward only when safe, and keep unrelated tracked, untracked and ignored
files. Re-read every cited path:line before editing. Implement, test and
document the prompt's scope. Run its local checks, with ESP-IDF only through
C:\esp\idf.cmd 6.0.1 and short build folders. Commit only owned files, push,
and confirm upstream equality. Report each evidence layer as
PASS/FAIL/NOT RUN/BLOCKED.
```

Whole set: the same text with "execute docs/prompts/01, 02 and 03 in order;
finish, commit and sync each prompt before starting the next; stop at the first
prompt that cannot meet its acceptance criteria and report why".

## Shared rules

1. **Authority.** Root `AGENTS.md` (CI `:3-19`, Git `:21-34`, the target and
   the planning note `:39-44`, PlatformIO `:50-54`, engineering `:100-141`, I2C
   `:145-167`, driver `:188-293`, versions `:297-311`), then `..\AGENTS.md` (CI
   `:3-19`, ESP-IDF `:24-38`), then this README, then the prompt.
   **[verify]** marks a fact or choice that execution must confirm against the
   named source or a build, and record in the handoff. When it proves false,
   follow the source and report the difference.
2. **Owner decisions, 2026-11-16.** These are final. A prompt reopens one only
   when a primary source contradicts it.
   - Prompt 01 records the scope election. Executing a prompt authorizes the
     edits it lists by path:line.
   - Pin ESP-IDF 6.0.1 for local builds and CI, targets `esp32s3` and `esp32s2`.
     The manifest declares `idf: ">=6.0.1"`. Do not claim any older version.
   - Use `examples/esp_idf/basic/` with `CMakeLists.txt`, `main/`,
     `sdkconfig.defaults*`, `README.md` and the fixed-name wrapper
     `components/RV3032/`. Do not use `override_path`. The console is the
     sdkconfig console with non-blocking `getchar()` (the OPT4001 recipe). Build
     identity comes from an example-only CMake header written by
     `main/GenerateBuildIdentity.cmake` (the INA228 pattern). The checker is
     `tools/check_idf_example_contract.py`, unit-tested with mutation cases in
     `tools/test_check_idf_example_contract.py`. Keep `docs/IDF_PORT.md` as the
     IDF guide.
   - Give both examples full command parity through one shared,
     framework-neutral CLI engine. Extract the engine first, keep the Arduino
     behavior and prove it with the existing checks.
   - Commit a real `idf_component.yml`. Replace its name-based ignore and
     package rejection with a content check. `idf_component.yml.orig` stays
     ignored and package-forbidden. In prompt 03, add `espidf` to
     `library.json` `frameworks` and state that PlatformIO
     `framework = espidf` is not tested; the package ships the component and
     the native example.
   - The IDF adapter's retry and recovery behavior matches the Wire adapter
     exactly: one physical attempt per callback, no recovery and no retry. The
     driver's internal FSM reset and bus clear after a timeout is SDK behavior,
     not a replayed write; document it.
   - CI builds the native example from the validated, extracted package in a
     folder not named after the repository. It fails, never skips, when the
     package is incomplete. Run the same build locally before the checkpoint.
   - Dispatch and wait for remote CI once, in prompt 03, on the exact final
     commit. Earlier prompts use local checks only. Pushes trigger CI
     automatically (`.github/workflows/ci.yml:3-8`). Do not wait for or cite
     those intermediate runs.
   - State the actual limits. The library's I2C callback timeout is 1..100 ms
     (`include/RV3032/Config.h:123-131`) and the primary-cell timeout 1..5 ms
     (`:161-167`). The example bus runs at 400 kHz
     (`examples/common/BoardConfig.h:36`) with a 50 ms default timeout (`:39`).
3. **Toolchain.** Run ESP-IDF only as `C:\esp\idf.cmd 6.0.1 <idf.py args>`
   with `-B C:\esp\build\rv3032-<target>-6.0.1` and
   `-D SDKCONFIG=C:\esp\build\rv3032-<target>-6.0.1.sdkconfig`. Never install
   ESP-IDF globally, run Espressif's installers, or set
   `IDF_PATH`/`IDF_TOOLS_PATH` in Windows settings. Run PlatformIO only as
   `.\scripts\pio.cmd`. After local IDF builds, delete
   `examples/esp_idf/basic/dependencies.lock` and `managed_components/`.
   `.gitignore` ignores them, but `pio pkg pack` does not apply `.gitignore`
   when `library.json` has an `export` field (PlatformIO 6.1.19
   `platformio/package/pack.py:249-255`).
4. **Evidence.** Report these six layers separately, each as PASS, FAIL,
   NOT RUN or BLOCKED, with the exact command, tool or SDK version, commit and
   tree state:
   - host tests;
   - static/contract checks;
   - native ESP-IDF build (S2/S3), where a linked image proves linking only;
   - package/clean consumer, including the native payload build;
   - remote CI (last prompt only);
   - physical hardware: NOT RUN.
   No board will be available for a long time, so HIL, flashing and console
   I/O on a target stay NOT RUN. A fake SDK or a linked image proves only its
   own layer.
5. **Git.** Work on `main` per `AGENTS.md:21-34`. No new branch, worktree,
   stash, discard, force-push or history rewrite. Stage only your own files or
   hunks. Commit and push each verified chunk and each completed prompt. To
   sync: fetch, integrate safely, push, fetch again, then confirm equal commit
   IDs with zero ahead and zero behind. A commit or fetch alone is not a sync.
   Record checkpoint SHAs in the handoff or in the next prompt's update, never
   in the commit itself. When subagents are used, only the lead does Git.
   Leave the ignored `RV3032-C7-audit-*.tar.gz` archives alone. Do not bump the
   version, tag or release.
6. **Status.** When a prompt is complete, set its row below to DONE in its
   final commit. Execution evidence goes in `CHANGELOG.md` `[Unreleased]` and
   the handoff, never in this README.
7. **After execution** this folder stays in `docs/prompts/` as executed
   historical requirements (TUSB32x precedent); never delete or archive it.
   The last prompt sets this header to `**Status: EXECUTED (historical
   requirements, not open work); hardware NOT RUN.**` and every row to DONE.
   Packages keep excluding `docs/prompts/`.

## Prompts

| # | Prompt | Requires | Delivers | Status |
|---|---|---|---|---|
| 01 | [Component, manifest and I2C adapter](01_COMPONENT_MANIFEST_AND_ADAPTER.md) | none | Scope election, root component, committed manifest with a content check, `i2c_master` adapter with host tests, S2/S3 bring-up builds | DONE |
| 02 | [Shared CLI engine and native example](02_SHARED_CLI_AND_NATIVE_EXAMPLE.md) | 01 DONE | Framework-neutral CLI engine with unchanged Arduino behavior, native shell with full parity, identity, sdkconfig, contract checker | NOT STARTED |
| 03 | [CI, package and closeout](03_CI_PACKAGE_AND_CLOSEOUT.md) | 02 DONE | IDF CI jobs (in-tree and packaged consumer), package and consumer gates, `espidf` framework, final docs, marking this set executed, the single remote-CI checkpoint | NOT STARTED |

## Out of scope

- Physical hardware or HIL execution, flashing, EEPROM writes and power
  cycling. These need separate fixture authorization
  (`docs/VERIFICATION.md:75-84`).
- PlatformIO `framework = espidf` builds. Other ESP-IDF versions, other targets
  and the ESP Component Registry. Releases, tags and version bumps.
- Bus managers, shared-bus arbitration, background tasks, ISR handling and
  new core APIs or core behavior. The one exception is a minimal warning fix
  that an IDF build proves necessary.
- Porting `test/test_hil*` to ESP-IDF, and editing sibling repositories, which
  are read-only references.
