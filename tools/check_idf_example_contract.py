#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import runpy
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

FORBIDDEN_TOKENS = [
    "ArduinoCompat",
    "IdfArduinoCompat",
    "Arduino.h",
    "Wire.h",
    "String",
    "Serial",
    "TwoWire",
    "examples/01_basic_bringup_cli/main.cpp",
]

REQUIRED_NATIVE_TOKENS = [
    'extern "C" void app_main(void)',
    "driver/i2c_master.h",
    "esp_timer_get_time",
    "vTaskDelay",
    "fgets",
    "i2c_new_master_bus",
]

FORBIDDEN_PLACEHOLDERS = [
    "Command is present in the native IDF contract",
    "use help for arguments",
    "Command is intentionally blocked in this native IDF example",
    "generic placeholder",
]

REQUIRED_CONFIRMATION_TOKENS = [
    "CONFIRM_TOKEN",
    "Confirmation required.",
    "Would change:",
    "Scope:",
    "Reason:",
    "Run exactly:",
]

REQUIRED_COMMAND_HANDLERS = [
    "set",
    "setbuild",
    "unix",
    "alarm_set",
    "alarm_match",
    "alarm_int",
    "alarm_clear",
    "timer",
    "clkout",
    "clkout_freq",
    "offset",
    "evi",
    "ts",
    "ts_reset",
    "status",
    "statusf",
    "status_clear",
    "validity",
    "ram",
    "ram_write",
    "reg",
    "eeprom",
    "backup",
    "clear_porf",
    "clear_vlf",
    "clear_bsf",
    "stress",
    "stress_mix",
    "selftest",
]

REQUIRED_CONFIRMING_FUNCTIONS = [
    "cmdSet",
    "cmdSetBuild",
    "cmdUnix",
    "cmdAlarmSet",
    "cmdAlarmMatch",
    "cmdAlarmInt",
    "cmdAlarmClear",
    "cmdTimer",
    "cmdClkout",
    "cmdClkoutFreq",
    "cmdOffset",
    "cmdEvi",
    "cmdTsReset",
    "cmdStatusClear",
    "cmdRamWrite",
    "cmdReg",
    "cmdBackup",
    "cmdClearPorf",
    "cmdClearVlf",
    "cmdClearBsf",
]


def fail(msg: str) -> None:
    print(f"IDF example contract FAILED: {msg}")
    raise SystemExit(1)


def main() -> int:
    ns = runpy.run_path(str(ROOT / "tools" / "check_cli_contract.py"))
    commands = ns.get("MANDATORY_COMMANDS", [])
    components = ns.get("IDF_REQUIRED_COMPONENTS", [])
    main_path = ROOT / "examples" / "espidf_basic" / "main" / "main.cpp"
    cmake_path = ROOT / "examples" / "espidf_basic" / "main" / "CMakeLists.txt"
    text = main_path.read_text(encoding="utf-8", errors="replace")
    cmake = cmake_path.read_text(encoding="utf-8", errors="replace")

    for token in FORBIDDEN_TOKENS:
        if token in text:
            fail(f"forbidden Arduino compatibility token in IDF example: {token}")
    for token in REQUIRED_NATIVE_TOKENS:
        if token not in text:
            fail(f"native ESP-IDF token missing: {token}")
    for token in FORBIDDEN_PLACEHOLDERS:
        if token in text:
            fail(f"generic placeholder text still present: {token}")
    for token in REQUIRED_CONFIRMATION_TOKENS:
        if token not in text:
            fail(f"confirmation dry-run output missing: {token}")
    for cmd in commands:
        if cmd == "?":
            if '"?"' not in text and " / ?" not in text and " | ?" not in text:
                fail("mandatory command '?' missing from IDF example")
        elif re.search(rf"\b{re.escape(cmd)}\b", text) is None:
            fail(f"mandatory command '{cmd}' missing from IDF example")
    for cmd in REQUIRED_COMMAND_HANDLERS:
        if f'strcmp(cmd, "{cmd}") == 0' not in text:
            fail(f"key command handler '{cmd}' missing from IDF dispatcher")
    for function in REQUIRED_CONFIRMING_FUNCTIONS:
        if re.search(rf"\b{re.escape(function)}\([^)]*bool confirmed[^)]*const char\* original", text) is None:
            fail(f"mutating handler '{function}' does not require confirmation context")
    for component in components:
        if re.search(rf"\b{re.escape(component)}\b", cmake) is None:
            fail(f"ESP-IDF CMake file missing component '{component}'")

    print("IDF example contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
