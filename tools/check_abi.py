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


def parse_err_values(header: str) -> dict[str, int]:
    match = re.search(
        r"enum\s+class\s+Err\s*:\s*uint8_t\s*\{(.*?)\}\s*;",
        header,
        re.DOTALL,
    )
    if match is None:
        raise ValueError("cannot locate Err enum")
    block = re.sub(r"/\*.*?\*/|//[^\n]*", "", match.group(1), flags=re.DOTALL)
    values: dict[str, int] = {}
    current = -1
    for entry in block.split(","):
        entry = entry.strip()
        if not entry:
            continue
        item = re.fullmatch(r"([A-Z][A-Z0-9_]*)(?:\s*=\s*(0[xX][0-9A-Fa-f]+|\d+))?", entry)
        if item is None:
            raise ValueError(f"unsupported Err declaration: {entry!r}")
        current = int(item.group(2), 0) if item.group(2) is not None else current + 1
        values[item.group(1)] = current
    return values


def header_integer(header: str, name: str) -> int | None:
    match = re.search(
        rf"\b{name}\s*=\s*(\d+)\s*;",
        re.sub(r"/\*.*?\*/|//[^\n]*", "", header, flags=re.DOTALL),
    )
    return None if match is None else int(match.group(1))


def main() -> int:
    errors: list[str] = []
    status_header = (ROOT / "include/RV3032/Status.h").read_text(encoding="utf-8")
    try:
        observed = parse_err_values(status_header)
        if observed != EXPECTED_ERR_VALUES:
            errors.append(f"Err value table changed: {observed}")
    except ValueError as exc:
        errors.append(str(exc))

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
