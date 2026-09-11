#!/usr/bin/env python3
from __future__ import annotations

import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
EXPECTED_ERR_VALUES = {
    "OK": 0,
    "NOT_INITIALIZED": 1,
    "INVALID_CONFIG": 2,
    "I2C_ERROR": 3,
    "TIMEOUT": 4,
    "INVALID_PARAM": 5,
    "INVALID_DATETIME": 6,
    "DEVICE_NOT_FOUND": 7,
    "EEPROM_WRITE_FAILED": 8,
    "REGISTER_READ_FAILED": 9,
    "REGISTER_WRITE_FAILED": 10,
    "QUEUE_FULL": 11,
    "BUSY": 12,
    "IN_PROGRESS": 13,
    "I2C_NACK_ADDR": 14,
    "I2C_NACK_DATA": 15,
    "I2C_TIMEOUT": 16,
    "I2C_BUS": 17,
    "EEPROM_VERIFY_FAILED": 18,
    "EEPROM_CLEANUP_FAILED": 19,
    "PRIMARY_CELL_ALREADY_ATTEMPTED": 20,
    "JOB_RESULT_UNAVAILABLE": 21,
    "INCOHERENT_DATA": 22,
    "CONFIGURATION_CLEANUP_FAILED": 23,
    "TRANSPORT_CONTRACT_VIOLATION": 24,
    "INTERNAL_STATE_ERROR": 25,
}


EXPECTED_ENUM_VALUES = {
    "DriverState": {"UNINIT": 0, "READY": 1, "DEGRADED": 2, "OFFLINE": 3},
    "TimestampSource": {"TLow": 0, "THigh": 1, "Evi": 2},
    "ClkoutFrequency": {"Hz32768": 0, "Hz1024": 1, "Hz64": 2, "Hz1": 3},
    "TimerFrequency": {"Hz4096": 0, "Hz64": 1, "Hz1": 2, "Hz1_60": 3},
    "EviDebounce": {"None": 0, "Hz256": 1, "Hz64": 2, "Hz8": 3},
    "PeriodicUpdateFrequency": {"SECOND": 0, "MINUTE": 1},
    "TrickleChargeMode": {"CHARGER_DISABLED": 0, "V1_75": 1, "V3_0": 2, "V4_5": 3},
    "TrickleChargeResistance": {"OHM_600": 0, "KOHM_2": 1, "KOHM_7": 2, "KOHM_12": 3},
    "ConfigurationEepromRegister": {
        "PMU": 0xC0, "OFFSET": 0xC1, "CLKOUT1": 0xC2, "CLKOUT2": 0xC3,
        "TEMPERATURE_REFERENCE0": 0xC4, "TEMPERATURE_REFERENCE1": 0xC5,
    },
    "ConfigurationFinalState": {
        "UNCHANGED": 0, "REQUESTED_VERIFIED": 1, "SAFE_DISABLED_VERIFIED": 2, "UNKNOWN": 3,
    },
    "PrimaryCellConfigurationOutcome": {
        "NOT_ATTEMPTED": 0, "ALREADY_CONFIGURED": 1, "EEPROM_UPDATED": 2, "FAILED": 3,
    },
    "PrimaryCellFailureStage": {
        "NONE": 0, "PRECONDITION": 1, "PREPARE_ACCESS": 2, "READ_PERSISTENT": 3,
        "WRITE_PERSISTENT": 4, "VERIFY_PERSISTENT": 5, "CLEANUP": 6, "SETTLE": 7,
    },
    "BackupSwitchMode": {"Off": 0, "Level": 1, "Direct": 2},
    "BackupChargePolicy": {"REQUIRE_CHARGER_OFF": 0, "ALLOW_BACKUP_CHARGING": 1},
    "Err": EXPECTED_ERR_VALUES,
}
# These defaults/sizes are compiled into consumer code. Changes require an
# intentional baseline update and release note, even if the ABI still links.
EXPECTED_PUBLIC_CONSTANTS = {
    "USER_EEPROM_SIZE": ("uint8_t", 32),
    "USER_EEPROM_JOB_MAX_BYTES": ("uint8_t", 16),
    "READ_TIME_OPERATION_TIMEOUT_MS": ("uint32_t", 200),
    "SET_TIME_OPERATION_TIMEOUT_MS": ("uint32_t", 700),
    "BACKUP_SWITCH_OPERATION_TIMEOUT_MS": ("uint32_t", 500),
    "BACKUP_SWITCH_OPERATION_TIMEOUT_MAX_MS": ("uint32_t", 1000),
    "PERSISTENT_ACCESS_RECOVERY_OPERATION_TIMEOUT_MS": ("uint32_t", 1000),
    "MIN_SET_TIME_OPERATION_BUDGET_MS": ("uint32_t", 125),
}
# Default arguments are also compiled into callers. Guard the declaration as
# well as its named constant so a replacement literal cannot bypass the check.
EXPECTED_OPERATION_TIMEOUT_DEFAULTS = {
    "startReadTimeSnapshotJob": "READ_TIME_OPERATION_TIMEOUT_MS",
    "startSetTimeAndClearInvalidFlagsVerifiedJob": "SET_TIME_OPERATION_TIMEOUT_MS",
    "startReadConfigurationEepromJob": "4000",
    "startReadUserEepromJob": "4000",
    "startWriteUserEepromJob": "6000",
    "startPersistentAccessStateRecoveryJob": "PERSISTENT_ACCESS_RECOVERY_OPERATION_TIMEOUT_MS",
    "startSetBackupSwitchModeJob": "BACKUP_SWITCH_OPERATION_TIMEOUT_MS",
    "startReadCoherentTemperatureJob": "READ_TIME_OPERATION_TIMEOUT_MS",
}


def parse_enum_values(header: str, name: str) -> dict[str, int]:
    # Doxygen examples or a retained old declaration must not supply the ABI
    # baseline when the active enum has changed.
    code = re.sub(r"/\*.*?\*/|//[^\n]*", "", header, flags=re.DOTALL)
    match = re.search(
        rf"enum\s+class\s+{re.escape(name)}\s*:\s*uint8_t\s*\{{(.*?)\}}\s*;",
        code,
        re.DOTALL,
    )
    if match is None:
        raise ValueError(f"cannot locate {name} enum with uint8_t representation")
    block = match.group(1)
    values: dict[str, int] = {}
    current = -1
    for entry in block.split(","):
        entry = entry.strip()
        if not entry:
            continue
        item = re.fullmatch(r"([A-Za-z_]\w*)(?:\s*=\s*(0[xX][0-9A-Fa-f]+|\d+))?", entry)
        if item is None:
            raise ValueError(f"unsupported {name} declaration: {entry!r}")
        current = int(item.group(2), 0) if item.group(2) is not None else current + 1
        values[item.group(1)] = current
    return values


def public_contract_errors(header: str) -> list[str]:
    errors: list[str] = []
    for name, expected in EXPECTED_ENUM_VALUES.items():
        try:
            observed = parse_enum_values(header, name)
            if observed != expected:
                errors.append(f"{name} value table changed: {observed}")
        except ValueError as exc:
            errors.append(str(exc))
    code = re.sub(r"/\*.*?\*/|//[^\n]*", "", header, flags=re.DOTALL)
    for name, (expected_type, expected_value) in EXPECTED_PUBLIC_CONSTANTS.items():
        match = re.search(
            rf"\bstatic\s+constexpr\s+(\w+)\s+{name}\s*=\s*(\d+)\s*;", code)
        if match is None or (match[1], int(match[2])) != (expected_type, expected_value):
            errors.append(f"public constant {name} changed (expected {expected_type} {expected_value})")
    defaults: dict[str, str] = {}
    for method in re.finditer(r"\bStatus\s+(\w+)\s*\(([^;{}]*)\)\s*;", code):
        argument = re.search(r"\buint32_t\s+operationTimeoutMs\s*=\s*([^,)]+)", method[2])
        if argument is not None:
            defaults[method[1]] = re.sub(r"\s+", "", argument[1])
    for name in sorted(defaults.keys() | EXPECTED_OPERATION_TIMEOUT_DEFAULTS.keys()):
        expected = EXPECTED_OPERATION_TIMEOUT_DEFAULTS.get(name)
        if defaults.get(name) != expected:
            errors.append(
                f"{name} operationTimeoutMs default changed "
                f"(expected {expected}, observed {defaults.get(name)})")
    return errors


def header_integer(header: str, name: str) -> int | None:
    match = re.search(
        rf"\b{name}\s*=\s*(\d+)\s*;",
        re.sub(r"/\*.*?\*/|//[^\n]*", "", header, flags=re.DOTALL),
    )
    return None if match is None else int(match.group(1))


def main() -> int:
    header = "\n".join((ROOT / "include/RV3032" / name).read_text(encoding="utf-8")
                       for name in ("Status.h", "Config.h", "RV3032.h"))
    errors = public_contract_errors(header)

    manifest = json.loads((ROOT / "library.json").read_text(encoding="utf-8"))
    version = manifest.get("version")
    if not isinstance(version, str) or re.fullmatch(r"\d+\.\d+\.\d+", version) is None:
        errors.append("library.json version must be MAJOR.MINOR.PATCH")
    else:
        parts = tuple(int(part) for part in version.split("."))
        version_header = (ROOT / "include/RV3032/Version.h").read_text(encoding="utf-8")
        if f'#define RV3032_VERSION_STRING "{version}"' not in version_header:
            errors.append("Version.h string does not match library.json")
        observed_parts = tuple(
            header_integer(version_header, name)
            for name in ("VERSION_MAJOR", "VERSION_MINOR", "VERSION_PATCH")
        )
        if observed_parts != parts:
            errors.append(
                f"Version.h components {observed_parts} do not match library.json {parts}"
            )
        expected_code = parts[0] * 10000 + parts[1] * 100 + parts[2]
        if header_integer(version_header, "VERSION_CODE") != expected_code:
            errors.append("Version.h VERSION_CODE does not match library.json")

    if errors:
        print("ABI contract FAILED:")
        for error in errors:
            print(f"- {error}")
        return 1
    print("ABI contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
