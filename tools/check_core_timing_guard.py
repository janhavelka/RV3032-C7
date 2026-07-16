#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCAN_DIRS = ("src", "include")
VALID_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp"}

FORBIDDEN_CALLS = {
    "millis": re.compile(r"\bmillis\s*\("),
    "micros": re.compile(r"\bmicros\s*\("),
    "delayMicroseconds": re.compile(r"\bdelayMicroseconds\s*\("),
    "yield": re.compile(r"\byield\s*\("),
}

INCLUDE_ARDUINO_RE = re.compile(r'^\s*#\s*include\s*[<\"]Arduino\.h[>\"]', re.MULTILINE)
BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
LINE_COMMENT_RE = re.compile(r"//[^\n]*")
STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')

def strip_non_code(text: str) -> str:
    text = BLOCK_COMMENT_RE.sub("", text)
    text = LINE_COMMENT_RE.sub("", text)
    return STRING_RE.sub('""', text)


def collect_sources() -> list[pathlib.Path]:
    files: list[pathlib.Path] = []
    for dirname in SCAN_DIRS:
        root = ROOT / dirname
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.is_file() and path.suffix.lower() in VALID_SUFFIXES:
                files.append(path)
    return files


def main() -> int:
    observed_calls: dict[str, dict[str, int]] = {}
    observed_includes: dict[str, int] = {}

    for path in collect_sources():
        rel = path.relative_to(ROOT).as_posix()
        raw = path.read_text(encoding="utf-8", errors="replace")
        code = strip_non_code(raw)

        call_counts: dict[str, int] = {}
        for call_name, pattern in FORBIDDEN_CALLS.items():
            count = len(pattern.findall(code))
            if count > 0:
                call_counts[call_name] = count
        if call_counts:
            observed_calls[rel] = call_counts

        include_count = len(INCLUDE_ARDUINO_RE.findall(raw))
        if include_count > 0:
            observed_includes[rel] = include_count

    errors: list[str] = []

    header = (ROOT / "include/RV3032/RV3032.h").read_text(
        encoding="utf-8", errors="replace"
    )
    status_header = (ROOT / "include/RV3032/Status.h").read_text(
        encoding="utf-8", errors="replace"
    )
    command_table = (ROOT / "include/RV3032/CommandTable.h").read_text(
        encoding="utf-8", errors="replace"
    )
    version_header = (ROOT / "include/RV3032/Version.h").read_text(
        encoding="utf-8", errors="replace"
    )
    version_generator = (ROOT / "scripts/generate_version.py").read_text(
        encoding="utf-8", errors="replace"
    )
    source = (ROOT / "src/RV3032.cpp").read_text(
        encoding="utf-8", errors="replace"
    )

    required_header_tokens = (
        "struct RawTransferResult",
        "struct TimedTransferResult",
        "RawTransferResult _i2cWriteReadRaw",
        "RawTransferResult _i2cWriteRaw",
        "TimedTransferResult readRegsBefore",
        "TimedTransferResult writeRegsBefore",
        "Status processPersistentJob(uint32_t& nowMs",
        "RV3032(const RV3032&) = delete",
        "RV3032(RV3032&&) = delete",
        "TEMP_LSB_FLAG_CLEAR",
        "TEMP_LSB_CLEAR_READ",
        "TEMP_LSB_CLEAR_WRITE",
        "enum class ConfigurationFinalState",
        "struct ConfigurationJobReport",
        "Status tick(uint32_t nowMs)",
        "Status startSetBackupSwitchModeJob(",
        "Status getSetTimerJobResult(",
        "Status getSetPeriodicUpdateJobResult(",
        "Status getSetBackupSwitchModeJobResult(",
        "Status getSetClkoutConfigJobResult(",
        "Status getSetTemperatureEventConfigJobResult(",
        "Status persistentOperationStatus = Status::Ok()",
        "Status persistentCleanupStatus = Status::Ok()",
        "Status _eepromOperationStatus = Status::Ok()",
        "Status _eepromCleanupStatus = Status::Ok()",
        "struct QuiescenceGuard",
    )
    for token in required_header_tokens:
        if token not in header:
            errors.append(f"missing Phase 1 header contract: {token!r}")

    required_source_tokens = (
        "normalizeTransportResult",
        "Transport callback returned invalid status",
        "readRegsBefore(",
        "writeRegsBefore(",
        "remainingBefore(",
        "earlierDeadline(",
        "EEPROM_CLEANUP_READY_TIMEOUT_MS +",
        "6U * i2cTimeoutMs + EEPROM_WRITE_SETTLE_MS",
        "persistentCleanupReserveMs(",
        "Password registers are unsupported",
        "Err::INTERNAL_STATE_ERROR",
        "Err::TRANSPORT_CONTRACT_VIOLATION",
        "registerMismatchDetail",
        "BACKUP_WAIT_ACTIVATION",
        "configurationCleanupWriteAttempted",
        "return pollEeprom(nowMs, 1, instructionsUsed);",
    )
    for token in required_source_tokens:
        if token not in source:
            errors.append(f"missing Phase 1 source contract: {token!r}")

    clkout_default_tokens = (
        "PMU_DEFAULT_ON_DELIVERY = 0x00",
        "CLKOUT1_DEFAULT_ON_DELIVERY = 0x00",
        "CLKOUT2_DEFAULT_ON_DELIVERY = 0x00",
    )
    for token in clkout_default_tokens:
        if token not in command_table:
            errors.append(f"missing exact CLKOUT factory default: {token!r}")
    if "constexpr uint8_t CLKOUT_PERSIST_INDEXES[] = {0, 2, 3};" not in source:
        errors.append("CLKOUT persistence must contain exactly C0, C2, and C3")

    deleted_symbols = (
        "PasswordCredential",
        "PasswordProtectionEvidence",
        "PasswordProtectionStatus",
        "unlockPasswordProtection",
        "startConfigurePasswordProtectionJob",
        "getPasswordProtectionStatus",
        "WritePassword",
        "ApplyPassword",
        "ApplyPasswordBytes",
        "ApplyPasswordCredential",
        "FinalizePasswordEnable",
        "CleanupLockPassword",
        "PasswordProtection",
        "persistentUseAddressList",
        "persistentAddresses",
        "currentCredential",
        "newCredential",
        "passwordEnable",
        "passwordAuthorizationMayBeActive",
        "_passwordStatus",
        "_beginInProgress",
        "beginInProgress",
        "ComparePersistent",
        "VerifyWriteFlags",
        "BeginVerifyPersistent",
        "SetTimerReadTimerHigh",
        "shouldTrackHealth",
        "binaryToBcd",
        "bcdToBinary",
        "Status RV3032::setBackupSwitchMode(",
        "ClkoutConfig clkoutConfig",
        "uint8_t clkoutGuard",
        "completedStatus",
        "registerUpdateRequiredMask",
        "registerUpdateForbiddenMask",
        "registerUpdateGuardedClear",
        "registerUpdateGuardFailureIsBusy",
        "firstVerified",
        "firstVerifiedValid",
        "_eepromLastStatus",
        "startEepromUpdate",
        "PRIMARY_CELL_WRITE_COMMAND_CAP",
        "writeCommandAttempts",
    )
    production_contract = header + "\n" + source
    for token in deleted_symbols:
        if token in production_contract:
            errors.append(f"deleted Phase 1 symbol remains in production: {token}")

    public_production_contract = (
        production_contract + "\n" + command_table + "\n" + version_header
    )
    for token, owner in (
        (r"\bTS_OVERWRITE_BIT\b", public_production_contract),
        (r"\bPMU_CLKOUT_DISABLE\b", public_production_contract),
        (r"\bVERSION_INT\b",
         public_production_contract + "\n" + version_generator),
    ):
        if re.search(token, owner):
            errors.append(f"removed compatibility alias remains: {token}")

    for token in (
        "DEPENDENCY_VERSION_TARGETS",
        "_render_dependency_versions_header",
        "DependencyVersions.h",
    ):
        if token in version_generator:
            errors.append(f"unrelated dependency-version generator remains: {token}")

    for token in (
        "REG_PASSWORD0", "REG_PASSWORD1", "REG_PASSWORD2", "REG_PASSWORD3",
        "REG_EEPROM_PASSWORD0", "REG_EEPROM_PASSWORD1",
        "REG_EEPROM_PASSWORD2", "REG_EEPROM_PASSWORD3",
        "REG_EEPROM_PW_ENABLE", "CONFIG_EEPROM_END",
    ):
        if token not in command_table:
            errors.append(f"required silicon-reference constant missing: {token}")

    if "_job.persistentAddress +\n                                _job.persistentIndex" not in source:
        errors.append("persistent addressing is not the required contiguous calculation")

    def function_body(start_token: str, end_token: str) -> str:
        start = source.find(start_token)
        end = source.find(end_token, start + 1)
        if start < 0 or end < 0:
            errors.append(f"cannot locate function contract: {start_token!r}")
            return ""
        return source[start:end]

    poll_job = function_body("Status RV3032::pollJob(", "Status RV3032::finishJob(")
    persistent = function_body(
        "Status RV3032::processPersistentJob(", "Status RV3032::processEeprom("
    )
    read_validator = function_body(
        "Status RV3032::validateReadRegsRequest(", "Status RV3032::readRegs("
    )
    plain_register_read = function_body(
        "Status RV3032::readRegs(", "Status RV3032::validateWriteRegsRequest("
    )
    write_validator = function_body(
        "Status RV3032::validateWriteRegsRequest(", "Status RV3032::writeRegs("
    )
    plain_register_write = function_body(
        "Status RV3032::writeRegs(",
        "bool RV3032::intersectsUnsupportedPasswordRange(",
    )
    timed_transport_read = function_body(
        "RV3032::TimedTransferResult RV3032::_i2cWriteReadTrackedBefore(",
        "RV3032::TimedTransferResult RV3032::_i2cWriteTrackedBefore(",
    )
    timed_transport_write = function_body(
        "RV3032::TimedTransferResult RV3032::_i2cWriteTrackedBefore(",
        "Status RV3032::_readRegisterRaw(",
    )
    timed_register_read = function_body(
        "RV3032::TimedTransferResult RV3032::readRegsBefore(",
        "RV3032::TimedTransferResult RV3032::writeRegsBefore(",
    )
    timed_register_write = function_body(
        "RV3032::TimedTransferResult RV3032::writeRegsBefore(",
        "Status RV3032::ensureRead(",
    )
    process_eeprom = function_body(
        "Status RV3032::processEeprom(", "bool RV3032::eepromQueueContains("
    )
    begin_body = function_body("Status RV3032::begin(", "Status RV3032::tick(")
    persistent_read_start = function_body(
        "Status RV3032::startPersistentReadJob(",
        "Status RV3032::startReadConfigurationEepromJob(",
    )
    persistent_write_start = function_body(
        "Status RV3032::startWriteUserEepromJob(",
        "Status RV3032::getPersistentReadJobResult(",
    )
    backup_start = function_body(
        "Status RV3032::startSetBackupSwitchModeJob(",
        "Status RV3032::getBackupSwitchMode(",
    )
    for name, body in (("pollJob", poll_job), ("processPersistentJob", persistent)):
        if re.search(r"\breadRegs\s*\(", body) or re.search(r"\bwriteRegs\s*\(", body):
            errors.append(f"{name} still dispatches through an untimed register helper")
    if re.search(r"instructionsUsed\s*\*\s*_config\.i2cTimeoutMs", source):
        errors.append("instruction-count time derivation remains")
    if "3700" in source or "timeoutMs > 600" in source:
        errors.append("old literal EEPROM reserve/cutoff logic remains")

    if not re.search(
        r"Status\s+RV3032::processPersistentJob\(uint32_t&\s*nowMs,\s*"
        r"bool&\s*callbackUsed\)",
        source,
    ):
        errors.append("persistent owner lost the mutable-now signature")
    for name, body in (("pollJob", poll_job), ("processEeprom", process_eeprom)):
        if "uint32_t currentNowMs = now_ms;" not in body:
            errors.append(f"{name} does not carry one mutable currentNowMs")
        if "processPersistentJob(currentNowMs, callbackUsed)" not in body:
            errors.append(f"{name} does not propagate mutable time into persistence")

    cleanup_formula = re.compile(
        r"constexpr\s+uint32_t\s+persistentCleanupReserveMs\("
        r"uint32_t\s+i2cTimeoutMs\)\s*\{\s*return\s+"
        r"EEPROM_CLEANUP_READY_TIMEOUT_MS\s*\+\s*"
        r"6U\s*\*\s*i2cTimeoutMs\s*\+\s*"
        r"EEPROM_WRITE_SETTLE_MS\s*;\s*\}"
    )
    if not cleanup_formula.search(source):
        errors.append("shared cleanup-reserve formula has drifted")
    if source.count("persistentCleanupReserveMs(") != 5:
        errors.append("all four cleanup-reserve owners must use the shared helper")
    if "GENERIC_EEPROM_OPERATION_TIMEOUT_MS - cleanupReserveMs" not in process_eeprom:
        errors.append("generic queue cutoff is not derived from its whole deadline")
    for name, body, timeout_name in (
        ("persistent read", persistent_read_start, "timeoutMs"),
        ("persistent write", persistent_write_start, "operationTimeoutMs"),
    ):
        if "cleanupReserveMs + _config.i2cTimeoutMs + 1U" not in body:
            errors.append(f"{name} admission omits forward-callback cleanup margin")
        if f"{timeout_name} - cleanupReserveMs" not in body:
            errors.append(f"{name} cutoff is not derived from the caller timeout")
    if "cleanupReserveMs + config.i2cTimeoutMs + 1U" not in begin_body:
        errors.append("begin() does not validate the generic cleanup reserve")

    for token in (
        "4U * _config.i2cTimeoutMs + activationMs + 1U",
        "operationTimeoutMs > BACKUP_SWITCH_OPERATION_TIMEOUT_MAX_MS",
        "operationTimeoutMs - (_config.i2cTimeoutMs + activationMs)",
    ):
        if token not in backup_start:
            errors.append(f"backup admission/cutoff contract drifted: {token!r}")

    def job_case_body(state: str) -> str:
        marker = f"case JobState::{state}:"
        start = poll_job.find(marker)
        if start < 0:
            errors.append(f"missing Phase 2 state case: {state}")
            return ""
        end = poll_job.find("case JobState::", start + len(marker))
        if end < 0:
            end = len(poll_job)
        return poll_job[start:end]

    phase2_graphs = {
        "timer": (
            [
                "TIMER_READ_CONTROL2_GUARD", "TIMER_READ_CONTROL1",
                "TIMER_WRITE_SAFE_CONTROL1", "TIMER_WRITE_PRESET",
                "TIMER_WRITE_FINAL_CONTROL1", "TIMER_VERIFY",
                "TIMER_CLEANUP_READ", "TIMER_CLEANUP_WRITE",
                "TIMER_CLEANUP_VERIFY",
            ],
            6,
            9,
        ),
        "periodic": (
            [
                "PERIODIC_READ_CONTROLS", "PERIODIC_WRITE_SAFE_CONTROL2",
                "PERIODIC_WRITE_USEL", "PERIODIC_WRITE_FINAL_CONTROL2",
                "PERIODIC_VERIFY", "PERIODIC_CLEANUP_READ",
                "PERIODIC_CLEANUP_WRITE", "PERIODIC_CLEANUP_VERIFY",
            ],
            5,
            8,
        ),
        "backup": (
            [
                "BACKUP_READ_CONTROL3", "BACKUP_READ_PMU",
                "BACKUP_WRITE_PMU", "BACKUP_VERIFY_PMU",
            ],
            4,
            4,
        ),
        "clkout": (
            [
                "CLKOUT_READ_ACTIVE", "CLKOUT_READ_GUARDS",
                "CLKOUT_WRITE_SAFE_CONFIG", "CLKOUT_WRITE_FINAL_PMU",
                "CLKOUT_VERIFY", "CLKOUT_CLEANUP_READ",
                "CLKOUT_CLEANUP_WRITE", "CLKOUT_CLEANUP_VERIFY",
            ],
            5,
            8,
        ),
        "temperature": (
            [
                "TEMPERATURE_READ_CONFIG",
                "TEMPERATURE_WRITE_SAFE_CONTROL3",
                "TEMPERATURE_WRITE_TIMESTAMP_CONTROL",
                "TEMPERATURE_WRITE_THRESHOLDS",
                "TEMPERATURE_WRITE_INTERRUPTS",
                "TEMPERATURE_WRITE_DETECTION", "TEMPERATURE_VERIFY",
                "TEMPERATURE_CLEANUP_READ", "TEMPERATURE_CLEANUP_WRITE",
                "TEMPERATURE_CLEANUP_VERIFY",
            ],
            7,
            10,
        ),
    }
    callback_tokens = (
        "readJob(", "writeConfigurationForward(",
        "writeConfigurationCleanup(", "writeRegsBefore(",
    )
    for graph_name, (states, success_cap, worst_cap) in phase2_graphs.items():
        positions = [poll_job.find(f"case JobState::{state}:") for state in states]
        if any(position < 0 for position in positions) or positions != sorted(positions):
            errors.append(f"{graph_name} source state order differs from Phase 2")
        if success_cap > len(states) or worst_cap != len(states):
            errors.append(f"{graph_name} source callback cap declaration is inconsistent")
        state_indexes = {state: index for index, state in enumerate(states)}
        for state in states:
            body = job_case_body(state)
            callback_count = sum(body.count(token) for token in callback_tokens)
            if callback_count != 1:
                errors.append(
                    f"{graph_name} state {state} must own exactly one callback, "
                    f"observed {callback_count}"
                )
            if poll_job.count(f"case JobState::{state}:") != 1:
                errors.append(f"{graph_name} state {state} is duplicated")
            for target in re.findall(
                r"_job\.state\s*=\s*JobState::([A-Z0-9_]+)", body
            ):
                if target in state_indexes and \
                   state_indexes[target] <= state_indexes[state]:
                    errors.append(
                        f"{graph_name} state {state} can re-enter forward state {target}"
                    )

    backup_wait = job_case_body("BACKUP_WAIT_ACTIVATION")
    if any(token in backup_wait for token in callback_tokens):
        errors.append("backup activation wait performs transport I/O")
    backup_write = job_case_body("BACKUP_WRITE_PMU")
    if backup_write.count("writeRegsBefore(") != 1 or \
       "configurationReport.mutationAttempted = true" not in backup_write:
        errors.append("backup requested PMU write is replayable or lacks dispatch evidence")
    backup_verify_body = job_case_body("BACKUP_VERIFY_PMU")
    if "writeConfigurationCleanup(" in backup_verify_body or \
       "writeRegsBefore(" in backup_verify_body:
        errors.append("backup reconciliation issues a cleanup/replay write")
    finish_backup_start = poll_job.find("auto finishBackupRequested")
    finish_backup_end = poll_job.find("Status st = Status::Ok();", finish_backup_start)
    finish_backup = poll_job[finish_backup_start:finish_backup_end]
    if finish_backup.find("ConfigurationFinalState::REQUESTED_VERIFIED") < 0 or \
       finish_backup.find("eepromQueuePush(") < 0 or \
       finish_backup.find("ConfigurationFinalState::REQUESTED_VERIFIED") > \
       finish_backup.find("eepromQueuePush("):
        errors.append("backup persistence is not ordered after requested-state proof")
    clkout_verify = job_case_body("CLKOUT_VERIFY")
    if clkout_verify.find("markConfigurationRequested();") < 0 or \
       clkout_verify.find("eepromQueuePush(") < 0 or \
       clkout_verify.find("markConfigurationRequested();") > \
       clkout_verify.find("eepromQueuePush("):
        errors.append("CLKOUT persistence is not ordered after requested-state proof")
    for name, body, validator, wrapper in (
        (
            "readRegsBefore", timed_register_read,
            "validateReadRegsRequest(", "_i2cWriteReadTrackedBefore(",
        ),
        (
            "writeRegsBefore", timed_register_write,
            "validateWriteRegsRequest(", "_i2cWriteTrackedBefore(",
        ),
    ):
        if validator not in body or wrapper not in body:
            errors.append(f"{name} bypasses shared validation or timed tracked transport")
        if re.search(r"_i2cWrite(?:Read)?Raw\s*\(", body) or "_updateHealth(" in body:
            errors.append(f"{name} directly owns raw dispatch or health tracking")
    for name, validator_body in (
        ("read", read_validator), ("write", write_validator)
    ):
        if "intersectsUnsupportedPasswordRange(reg, len)" not in validator_body:
            errors.append(f"shared {name} validator omits password-range denial")
    if "validateReadRegsRequest(" not in plain_register_read:
        errors.append("plain readRegs bypasses the shared read validator")
    if "validateWriteRegsRequest(" not in plain_register_write:
        errors.append("plain writeRegs bypasses the shared write validator")
    if "_i2cWriteReadRaw(" not in timed_transport_read or "_updateHealth(" not in timed_transport_read:
        errors.append("timed read tracked wrapper does not own raw dispatch and health")
    if "_i2cWriteRaw(" not in timed_transport_write or "_updateHealth(" not in timed_transport_write:
        errors.append("timed write tracked wrapper does not own raw dispatch and health")

    for name, body, raw_call in (
        ("timed read", timed_transport_read, "_i2cWriteReadRaw("),
        ("timed write", timed_transport_write, "_i2cWriteRaw("),
    ):
        required_timing_tokens = (
            "remainingMs <= 1U",
            "(remainingMs - 1U)",
            "nowMs += timeoutMs;",
            "result.completedAtMs = nowMs;",
            "result.deadlineCrossed = !remainingBefore",
            "Transport callback exceeded timeout",
            "Transport callback crossed operation deadline",
        )
        for token in required_timing_tokens:
            if token not in body:
                errors.append(f"{name} omits post-callback timing contract: {token!r}")
        raw_pos = body.find(raw_call)
        first_clock = body.find("_nowMs()")
        last_clock = body.rfind("_nowMs()")
        if raw_pos < 0 or first_clock < 0 or last_clock <= raw_pos or first_clock >= raw_pos:
            errors.append(f"{name} does not sample the clock before and after raw dispatch")
        decision = body.find("if (!raw.callbackInvoked)")
        precedence = [
            body.find("raw.status.code == Err::TRANSPORT_CONTRACT_VIOLATION", decision),
            body.find("else if (result.callbackTimeoutViolated)", decision),
            body.find("else if (!raw.status.ok())", decision),
            body.find("else if (result.deadlineCrossed)", decision),
        ]
        if decision < 0 or any(position < 0 for position in precedence) or precedence != sorted(precedence):
            errors.append(f"{name} effective-result precedence has drifted")

    for token in (
        "earlierDeadline(\n          nowMs, boundary, _job.persistentPhaseDeadlineMs)",
        "earlierDeadline(nowMs, boundary, _job.mutationCutoffMs)",
    ):
        if token not in persistent:
            errors.append("persistent transfer owner omits an earliest applicable boundary")

    if "calendarWriteStatus = transfer.callbackStatus" not in poll_job or \
       "statusWriteStatus = transfer.callbackStatus" not in poll_job:
        errors.append("verified calendar report does not retain exact callback status")

    if source.count("else if (st.ok() || st.inProgress())") != 2 or \
       "Status terminal = st;" not in process_eeprom:
        errors.append(
            "persistent outer owner can overwrite an effective callback failure "
            "at the whole deadline"
        )
    begin_cleanup = re.search(
        r"auto beginCleanup = \[&\]\(\) \{(.*?)\n  \};",
        persistent,
        re.DOTALL,
    )
    if begin_cleanup is None or not re.search(
        r"_job\.persistentCleanupRequired\s*=\s*true;.*?"
        r"_job\.persistentState\s*=",
        begin_cleanup.group(1) if begin_cleanup is not None else "",
        re.DOTALL,
    ):
        errors.append(
            "persistent cleanup owner does not latch mandatory proof before state admission"
        )

    tracked_transport = "\n".join(
        (
            function_body("Status RV3032::_i2cWriteReadTracked(",
                          "Status RV3032::_i2cWriteTracked("),
            function_body("Status RV3032::_i2cWriteTracked(",
                          "Status RV3032::_i2cWriteReadTrackedTimeout("),
            function_body("Status RV3032::_i2cWriteReadTrackedTimeout(",
                          "Status RV3032::_i2cWriteTrackedTimeout("),
            function_body("Status RV3032::_i2cWriteTrackedTimeout(",
                          "Status RV3032::_i2cWriteReadPresenceTracked("),
            function_body("Status RV3032::_i2cWriteReadPresenceTracked(",
                          "RV3032::TimedTransferResult RV3032::_i2cWriteReadTrackedBefore("),
            timed_transport_read,
            timed_transport_write,
        )
    )
    def strip_cpp_comments(text: str) -> str:
        text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
        return re.sub(r"//[^\n]*", "", text)

    health_call_re = re.compile(r"(?<!::)\b_updateHealth\s*\(")
    all_health_calls = len(health_call_re.findall(strip_cpp_comments(source)))
    tracked_health_calls = len(
        health_call_re.findall(strip_cpp_comments(tracked_transport))
    )
    if all_health_calls != tracked_health_calls:
        errors.append("health update exists outside a tracked transport wrapper")
    if not re.search(
        r"case JobState::IDLE:\s*default:\s*"
        r"st = Status::Error\(\s*Err::INTERNAL_STATE_ERROR,.*?"
        r"return finishJob\(st\);",
        poll_job,
        re.DOTALL,
    ):
        errors.append("ordinary impossible-state route is not a terminal internal error")
    backup_verify = re.search(
        r"case JobState::BACKUP_VERIFY_PMU:\s*\{(.*?)"
        r"case JobState::BACKUP_WAIT_ACTIVATION:",
        poll_job,
        re.DOTALL,
    )
    if backup_verify is None or not re.search(
        r"if \(!_job\.configurationReport\.mutationAttempted\).*?"
        r"Err::INTERNAL_STATE_ERROR",
        backup_verify.group(1) if backup_verify is not None else "",
        re.DOTALL,
    ):
        errors.append(
            "backup verification without mutation evidence can return success"
        )
    idle_impossible = poll_job[
        poll_job.find("if (!isJobBusy())"):
        poll_job.find("if (maxInstructions == 0)")
    ]
    default_impossible = job_case_body("IDLE")
    for name, body in (
        ("idle", idle_impossible),
        ("default", default_impossible),
    ):
        if not re.search(
            r"activeKind == JobKind::SET_BACKUP_SWITCH_MODE.*?"
            r"finalState ==\s*ConfigurationFinalState::REQUESTED_VERIFIED.*?"
            r"finishJob\(_job\.configurationReport\.operationStatus\)",
            body,
            re.DOTALL,
        ):
            errors.append(
                f"backup requested proof can restart reconciliation from {name} impossible state"
            )
    if poll_job.count("const bool cleanupAlreadyEntered =") != 2 or not all(
        token in poll_job for token in (
            "Impossible state during configuration cleanup",
            "_job.configurationReport.finalState =\n"
            "              ConfigurationFinalState::UNKNOWN",
            "_job.state = configurationRecoveryState(_job.activeKind)",
            "_job.persistentCleanupRequired",
            "_job.state = JobState::PERSISTENT",
        )
    ):
        errors.append(
            "impossible active/cleanup states can bypass or restart reconciliation"
        )
    if not all(token in persistent for token in (
        "case EepromState::IDLE:",
        "Err::INTERNAL_STATE_ERROR",
        "if (cleanupState)",
        "return finishCleanupFailure(internal);",
        "if (_job.persistentCleanupRequired)",
        "rememberFailure(internal);",
        "return finishOperation(internal);",
    )):
        errors.append("persistent impossible-state route does not preserve bounded cleanup")

    if re.search(r"_totalSuccess\s*<\s*UINT32_MAX", source):
        errors.append("total-success counter still saturates")
    if re.search(r"_totalFailures\s*<\s*UINT32_MAX", source):
        errors.append("total-failure counter still saturates")
    if re.search(r"<\s*UINT32_MAX", source):
        errors.append("a uint32_t counter saturation guard remains")
    if source.count("++_totalSuccess;") != 1 or source.count("++_totalFailures;") != 1:
        errors.append("lifetime health counters must use one ordinary uint32_t increment each")

    if "CONFIGURATION_CLEANUP_FAILED = 23" not in status_header:
        errors.append("Err value 23 contract missing")
    if "TRANSPORT_CONTRACT_VIOLATION = 24" not in status_header:
        errors.append("Err value 24 contract missing")
    if "INTERNAL_STATE_ERROR = 25" not in status_header:
        errors.append("Err value 25 contract missing")

    end_match = re.search(
        r"void\s+RV3032::end\s*\(\s*\)\s*\{\s*_resetRuntimeState\(\);\s*\}",
        source,
    )
    if end_match is None:
        errors.append("end() is not unconditional zero-I/O runtime reset")

    enum_blocks = re.findall(
        r"enum class (EepromState|JobKind|JobState)\s*:\s*uint8_t\s*\{(.*?)\};",
        header,
        re.DOTALL,
    )
    if len(enum_blocks) != 3:
        errors.append("private cooperative enum blocks not found")
    else:
        expected_enums = {
            "EepromState": [
                "IDLE", "READ_CONTROL1", "ENABLE_EERD", "VERIFY_EERD",
                "WAIT_READY", "READ_ACTIVE_C0", "WRITE_SAFE_C0",
                "VERIFY_SAFE_C0", "WRITE_ADDR", "VERIFY_ADDR",
                "WRITE_SENTINEL1", "VERIFY_SENTINEL1", "PRE_READ_BUSY1",
                "READ_CMD1", "WAIT_READ1", "POLL_READ1", "READ_DATA1",
                "WRITE_SENTINEL2", "VERIFY_SENTINEL2", "PRE_READ_BUSY2",
                "READ_CMD2", "WAIT_READ2", "POLL_READ2", "READ_DATA2",
                "CLEAR_EEF", "VERIFY_EEF", "WRITE_DATA", "VERIFY_DATA",
                "WAIT_READY_PRE_CMD", "WRITE_CMD", "WAIT_WRITE_SETTLE",
                "WAIT_READY_POST_CMD", "CLEANUP_WAIT_READY",
                "RESTORE_SELECTED_ACTIVE", "VERIFY_SELECTED_ACTIVE",
                "RESTORE_ACTIVE", "VERIFY_ACTIVE", "RESTORE_CONTROL1",
                "VERIFY_CONTROL1", "SETTLE",
            ],
            "JobKind": [
                "NONE", "SET_TIMER", "SET_PERIODIC_UPDATE",
                "SET_BACKUP_SWITCH_MODE", "SET_CLKOUT_CONFIG",
                "SET_TEMPERATURE_EVENT_CONFIG", "REGISTER_UPDATE",
                "TEMP_LSB_FLAG_CLEAR", "WRITE_USER_RAM",
                "READ_COHERENT_TEMPERATURE", "READ_TIME_SNAPSHOT",
                "SET_TIME_VERIFIED", "PERSISTENT_READ", "USER_EEPROM_WRITE",
            ],
            "JobState": [
                "IDLE", "TIMER_READ_CONTROL2_GUARD", "TIMER_READ_CONTROL1",
                "TIMER_WRITE_SAFE_CONTROL1", "TIMER_WRITE_PRESET",
                "TIMER_WRITE_FINAL_CONTROL1", "TIMER_VERIFY",
                "TIMER_CLEANUP_READ", "TIMER_CLEANUP_WRITE",
                "TIMER_CLEANUP_VERIFY", "PERIODIC_READ_CONTROLS",
                "PERIODIC_WRITE_SAFE_CONTROL2", "PERIODIC_WRITE_USEL",
                "PERIODIC_WRITE_FINAL_CONTROL2", "PERIODIC_VERIFY",
                "PERIODIC_CLEANUP_READ", "PERIODIC_CLEANUP_WRITE",
                "PERIODIC_CLEANUP_VERIFY", "BACKUP_READ_CONTROL3",
                "BACKUP_READ_PMU", "BACKUP_WRITE_PMU", "BACKUP_VERIFY_PMU",
                "BACKUP_WAIT_ACTIVATION", "REGISTER_UPDATE_READ",
                "REGISTER_UPDATE_WRITE", "REGISTER_UPDATE_READ_QUIESCENCE",
                "TEMP_LSB_CLEAR_READ",
                "TEMP_LSB_CLEAR_WRITE", "EVI_RESET_READ",
                "EVI_RESET_WRITE_CLEAR", "EVI_RESET_WRITE_SET",
                "REGISTER_BLOCK_UPDATE_READ", "REGISTER_BLOCK_UPDATE_WRITE",
                "CLKOUT_READ_ACTIVE", "CLKOUT_READ_GUARDS",
                "CLKOUT_WRITE_SAFE_CONFIG", "CLKOUT_WRITE_FINAL_PMU",
                "CLKOUT_VERIFY", "CLKOUT_CLEANUP_READ",
                "CLKOUT_CLEANUP_WRITE", "CLKOUT_CLEANUP_VERIFY",
                "TEMPERATURE_READ_CONFIG", "TEMPERATURE_WRITE_SAFE_CONTROL3",
                "TEMPERATURE_WRITE_TIMESTAMP_CONTROL",
                "TEMPERATURE_WRITE_THRESHOLDS",
                "TEMPERATURE_WRITE_INTERRUPTS",
                "TEMPERATURE_WRITE_DETECTION", "TEMPERATURE_VERIFY",
                "TEMPERATURE_CLEANUP_READ", "TEMPERATURE_CLEANUP_WRITE",
                "TEMPERATURE_CLEANUP_VERIFY", "WRITE_USER_RAM_CHUNK",
                "READ_TEMPERATURE_FIRST", "READ_TEMPERATURE_SECOND",
                "READ_TIME_STATUS", "READ_TIME_CALENDAR",
                "SET_TIME_READ_STATUS_BEFORE", "SET_TIME_WRITE_CALENDAR",
                "SET_TIME_VERIFY_CALENDAR", "SET_TIME_READ_STATUS_BEFORE_CLEAR",
                "SET_TIME_WRITE_STATUS", "SET_TIME_READ_STATUS_AFTER",
                "SET_TIME_READ_FINAL_CALENDAR", "PERSISTENT",
            ],
        }
        for enum_name, block in enum_blocks:
            values = re.findall(r"^\s*([A-Za-z][A-Za-z0-9_]*)\s*,?\s*$", block, re.MULTILINE)
            mixed = [value for value in values if value != value.upper()]
            if mixed:
                errors.append(f"mixed-case private enum values remain: {mixed}")
            if values != expected_enums[enum_name]:
                errors.append(
                    f"{enum_name} set/order differs from the Phase 2 contract: {values}"
                )

    for rel, counts in observed_calls.items():
        errors.append(f"forbidden timing calls in {rel}: {counts}")

    for rel, count in observed_includes.items():
        errors.append(f"forbidden Arduino include in {rel}: count={count}")

    if errors:
        print("Core timing guard FAILED:")
        for err in errors:
            print(f"- {err}")
        return 1

    print("Core timing guard PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
