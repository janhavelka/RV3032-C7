#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

REQUIRED_COMMON = [
    "BoardConfig.h",
    "BuildConfig.h",
    "Log.h",
    "I2cTransport.h",
    "I2cScanner.h",
    "CommandHandler.h",
    "TransportAdapter.h",
    "BusDiag.h",
    "CliShell.h",
    "CliStyle.h",
    "HealthView.h",
    "HealthDiag.h",
]

MANDATORY_COMMANDS = [
    "help", "scan", "probe", "recover", "drv", "read", "verbose", "stress",
    "primary-cell",
]


def fail(msg: str) -> None:
    print(f"CLI contract FAILED: {msg}")
    raise SystemExit(1)


def ensure_exists(path: pathlib.Path, label: str) -> None:
    if not path.exists():
        fail(f"missing {label}: {path.as_posix()}")


def ensure_missing(path: pathlib.Path, label: str) -> None:
    if path.exists():
        fail(f"forbidden {label} still present: {path.as_posix()}")


def main() -> int:
    common_dir = ROOT / "examples" / "common"
    bringup_main = ROOT / "examples" / "01_basic_bringup_cli" / "main.cpp"
    hil_runner = ROOT / "tools" / "hil_cli_runner.py"

    ensure_exists(common_dir, "common example directory")
    ensure_exists(bringup_main, "bringup CLI example")
    ensure_exists(hil_runner, "HIL runner")

    ensure_missing(ROOT / "examples" / "00_smoke_boot", "deprecated example 00_smoke_boot")
    ensure_missing(
        ROOT / "examples" / "03_feature_walkthrough",
        "deprecated example 03_feature_walkthrough",
    )

    for name in REQUIRED_COMMON:
        ensure_exists(common_dir / name, f"common helper {name}")

    text = bringup_main.read_text(encoding="utf-8", errors="replace")
    command_text = (common_dir / "CommandHandler.h").read_text(
        encoding="utf-8", errors="replace"
    )
    shell_text = (common_dir / "CliShell.h").read_text(
        encoding="utf-8", errors="replace"
    )
    native_text = (ROOT / "test" / "test_native" / "test_datetime.cpp").read_text(
        encoding="utf-8", errors="replace"
    )

    for cmd in MANDATORY_COMMANDS:
        if re.search(rf"\b{re.escape(cmd)}\b", text) is None:
            fail(f"mandatory command '{cmd}' missing in {bringup_main.as_posix()}")

    if re.search(r"\bcfg\b", text) is None and re.search(r"\bsettings\b", text) is None:
        fail("either 'cfg' or 'settings' command must be present")

    required_contract = [
        "primary-cell ensure CONFIRM-PRIMARY-CELL",
        "cfg.enableEepromWrites = false",
        "cfg.waitMs = rtc_wait_ms",
        "g_rtc.probe()",
        "PendingSurface::ORDINARY_JOB",
        "PendingSurface::EEPROM",
        "pollPendingOperation(millis())",
        "g_rtc.pollJob(nowMs, 1, instructionsUsed)",
        "g_rtc.pollEeprom(nowMs, 1, instructionsUsed)",
        "g_rtc.isEepromBusy()",
        "status.inProgress() || g_rtc.isEepromBusy()",
        "g_pendingOperation = PendingOperation{}",
        "cli_shell::readLine(line)",
        "startSetBackupSwitchModeJob",
        "getSetBackupSwitchModeJobResult",
        "ts.timeValid",
        "empty/unset",
        "write_attempted=",
        "cleanup_status=",
        "persistent_target_verified=",
        "active_target_verified=",
        "timer <ticks 1..4095>",
    ]
    for token in required_contract:
        if token not in text:
            fail(f"passive/provisioning contract token missing: {token!r}")

    exact_pending_type = re.compile(
        r"enum class PendingSurface\s*:\s*uint8_t\s*\{\s*NONE,\s*"
        r"ORDINARY_JOB,\s*EEPROM\s*\};.*?struct PendingOperation\s*\{.*?"
        r"PendingSurface surface\s*=\s*PendingSurface::NONE;.*?"
        r"const char\* name\s*=\s*nullptr;.*?"
        r"RV3032::Status ordinaryStatus\s*=\s*RV3032::Status::Ok\(\);.*?\};",
        re.DOTALL,
    )
    if exact_pending_type.search(text) is None:
        fail("exact fixed PendingSurface/PendingOperation owner is missing")

    for token in (
        "parseU8Token",
        "parseU16Token",
        "parseU32Token",
        "parseInt32Token",
        "parseFloatToken",
        "parseBool01Token",
        "parseRegisterToken",
        "errno = 0",
        "ERANGE",
        "std::isfinite",
        "strtod",
    ):
        if token not in command_text:
            fail(f"strict token-parser contract missing: {token!r}")

    for token in ("overflowed", "buffer = \"\"", "outLine.trim()"):
        if token not in shell_text:
            fail(f"overflow-discarding line-reader contract missing: {token!r}")

    forbidden_parser_patterns = {
        "local read_line": r"\bread_line\s*\(",
        "obsolete cmd reader": r"\bcmd::readLine\b|\bbool\s+readLine\s*\(char\*",
        "obsolete parseInt": r"\bparseInt\s*\(",
        "atoi": r"\batoi\s*\(",
        "String.toInt": r"\.toInt\s*\(",
        "String.toFloat": r"\.toFloat\s*\(",
        "sscanf": r"\bsscanf\s*\(",
        "unconditional tick": r"g_rtc\.tick\s*\(",
        "inferred job owner": r"g_rtc\.isJobBusy\s*\(",
    }
    combined_cli = text + "\n" + command_text
    for label, pattern in forbidden_parser_patterns.items():
        if re.search(pattern, combined_cli):
            fail(f"forbidden permissive/parallel CLI path remains: {label}")

    for token in (
        "test_phase3_strict_cli_numeric_tokens_preserve_outputs",
        "test_phase3_cli_line_reader_discards_overflow_through_terminator",
        "test_phase3_cli_invalid_mutating_commands_are_zero_io",
        "test_phase3_cli_ram_and_timestamp_terminal_output",
        "test_phase3_cli_owner_handoff_is_single_callback_and_preserves_status",
        "test_phase3_cli_persistent_helper_deadline_does_not_orphan",
        '#include "examples/01_basic_bringup_cli/main.cpp"',
        "compileMaintainedReadmePollingSnippets",
    ):
        if token not in native_text:
            fail(f"native CLI/README evidence missing: {token!r}")

    if "pollJob(deadlineMs, 1, used)" not in text:
        fail("typed persistent helper lacks exact-deadline terminal poll")
    if "if (g_pendingOperation.surface == PendingSurface::NONE)" not in text:
        fail("CLI prompt is not deferred while cooperative work is owned")
    for token in (
        "Build timestamp calendar set completed",
        'enable ? "CLKOUT enable" : "CLKOUT disable"',
        "operationAccepted(operationName, st)",
    ):
        if token not in text:
            fail(f"truthful terminal/typed-result ownership token missing: {token!r}")

    for token in ("verified calendar set", "verified Unix calendar set"):
        if token in text:
            fail(f"synchronous calendar command retains false completion claim: {token!r}")

    forbidden_contract = [
        "setPrimaryBatteryBackupDefaults",
        "cfg.backupMode",
        "REG_EE_COMMAND",
        "backup usual",
    ]
    for token in forbidden_contract:
        if token in text:
            fail(f"unsafe legacy provisioning token remains: {token!r}")

    hil_text = hil_runner.read_text(encoding="utf-8", errors="replace")
    for token in (
        '"timer 1 2 0"',
        '"user RAM write terminal status: OK"',
        '"ram-acceptance-only"',
    ):
        if token not in hil_text:
            fail(f"truthful HIL terminal-evidence token missing: {token!r}")
    if '"timer 0 2 0"' in hil_text:
        fail("HIL runner retains timer ticks=0 contrary to the public range")
    for token in (
        "--authorization-port",
        "--authorization-module",
        "--authorization-primary-cell-chemistry",
        "--authorization-power-conditions",
        "CONFIRM-POSSIBLE-C0-WRITE",
        "--authorization-vdd-off-backfeed-scope",
        "destructive_authorization_record(args)",
    ):
        if token not in hil_text:
            fail(f"destructive HIL authorization token missing: {token!r}")

    print("CLI contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
