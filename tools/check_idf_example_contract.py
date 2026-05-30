#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import json
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
EXAMPLE_DIR = ROOT / "examples" / "esp_idf" / "basic"
MAIN_CPP = EXAMPLE_DIR / "main" / "main.cpp"
PROJECT_CMAKE = EXAMPLE_DIR / "CMakeLists.txt"
MAIN_CMAKE = EXAMPLE_DIR / "main" / "CMakeLists.txt"
ROOT_CMAKE = ROOT / "CMakeLists.txt"
IDF_COMPONENT_YML = ROOT / "idf_component.yml"
LIBRARY_JSON = ROOT / "library.json"

FORBIDDEN_PATTERNS = {
    "Arduino token": re.compile(r"\bArduino\b"),
    "Arduino include": re.compile(r"#\s*include\s*[<\"]Arduino\.h[>\"]"),
    "Wire include": re.compile(r"#\s*include\s*[<\"]Wire\.h[>\"]"),
    "ArduinoCompat token": re.compile(r"\bArduinoCompat\b"),
    "IdfArduinoCompat token": re.compile(r"\bIdfArduinoCompat\b"),
    "String token": re.compile(r"\bString\b"),
    "Serial token": re.compile(r"\bSerial\b"),
    "TwoWire token": re.compile(r"\bTwoWire\b"),
    "Wire token": re.compile(r"\bWire\b"),
    "millis call": re.compile(r"\bmillis\s*\("),
    "micros call": re.compile(r"\bmicros\s*\("),
    "delay call": re.compile(r"\bdelay\s*\("),
    "delayMicroseconds call": re.compile(r"\bdelayMicroseconds\s*\("),
    "yield call": re.compile(r"\byield\s*\("),
    "F macro": re.compile(r"\bF\s*\("),
    "ARDUINO token": re.compile(r"\bARDUINO\b"),
    "ARDUINO_ARCH_ESP32 token": re.compile(r"\bARDUINO_ARCH_ESP32\b"),
    "CONFIG_ARDUINO token": re.compile(r"\bCONFIG_ARDUINO\b"),
    "examples/common dependency": re.compile(r"examples[/\\]common"),
    "Arduino CLI dependency": re.compile(r"examples[/\\]01_basic_bringup_cli[/\\]main\.cpp"),
    "old placeholder command text": re.compile(r"Command is present in the native IDF contract"),
    "old blocked command text": re.compile(r"Command is intentionally blocked in this native IDF example"),
    "generic placeholder": re.compile(r"generic placeholder"),
    "placeholder help text": re.compile(r"use help for arguments"),
}

REQUIRED_MAIN_PATTERNS = {
    "app_main entry point": re.compile(r'extern\s+"C"\s+void\s+app_main\s*\(\s*void\s*\)'),
    "native I2C master header": re.compile(r"driver/i2c_master\.h"),
    "I2C bus creation": re.compile(r"\bi2c_new_master_bus\b"),
    "I2C device add": re.compile(r"\bi2c_master_bus_add_device\b"),
    "I2C write": re.compile(r"\bi2c_master_transmit\b"),
    "I2C write-read": re.compile(r"\bi2c_master_transmit_receive\b"),
    "ESP-IDF monotonic time": re.compile(r"\besp_timer_get_time\b"),
    "task scheduling": re.compile(r"\bvTaskDelay\b"),
    "driver config": re.compile(r"\bRV3032::Config\b"),
    "write callback assignment": re.compile(r"\bi2cWrite\b"),
    "write-read callback assignment": re.compile(r"\bi2cWriteRead\b"),
    "transport context assignment": re.compile(r"\bi2cUser\b"),
    "begin call": re.compile(r"\bbegin\s*\("),
    "tick call": re.compile(r"\btick\s*\("),
    "end call": re.compile(r"\bend\s*\("),
}

REQUIRED_MAIN_CMAKE = {
    "idf_component_register": re.compile(r"\bidf_component_register\s*\("),
    "main.cpp": re.compile(r"\bmain\.cpp\b"),
    "ESP-IDF I2C driver dependency": re.compile(r"\besp_driver_i2c\b"),
    "esp_timer dependency": re.compile(r"\besp_timer\b"),
}

REQUIRED_ROOT_CMAKE = {
    "idf_component_register": re.compile(r"\bidf_component_register\s*\("),
    "core source": re.compile(r'"src/RV3032\.cpp"'),
    "public include dir": re.compile(r'"include"'),
}

FORBIDDEN_CMAKE = {
    "arduino dependency": re.compile(r"\barduino\b", re.IGNORECASE),
    "esp32-arduino dependency": re.compile(r"\besp32-arduino\b", re.IGNORECASE),
}

FORBIDDEN_ROOT_CMAKE = {
    "example dependency": re.compile(r"\bexamples[/\\]"),
    "I2C driver dependency": re.compile(r"\besp_driver_i2c\b|\bdriver\b"),
    "ESP timer dependency": re.compile(r"\besp_timer\b"),
    "FreeRTOS dependency": re.compile(r"\bfreertos\b", re.IGNORECASE),
    "Arduino dependency": re.compile(r"\barduino\b", re.IGNORECASE),
}


def fail(message: str) -> None:
    print(f"IDF example contract FAILED: {message}")
    raise SystemExit(1)


def read(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def iter_example_files() -> list[pathlib.Path]:
    return [path for path in EXAMPLE_DIR.rglob("*") if path.is_file()]


def main() -> int:
    if not MAIN_CPP.exists():
        fail(f"missing ESP-IDF example main: {MAIN_CPP.relative_to(ROOT).as_posix()}")
    if not PROJECT_CMAKE.exists():
        fail(f"missing ESP-IDF project CMakeLists: {PROJECT_CMAKE.relative_to(ROOT).as_posix()}")
    if not MAIN_CMAKE.exists():
        fail(f"missing ESP-IDF main CMakeLists: {MAIN_CMAKE.relative_to(ROOT).as_posix()}")
    if not ROOT_CMAKE.exists():
        fail(f"missing root ESP-IDF component CMakeLists: {ROOT_CMAKE.relative_to(ROOT).as_posix()}")
    if not IDF_COMPONENT_YML.exists():
        fail(f"missing ESP-IDF component manifest: {IDF_COMPONENT_YML.relative_to(ROOT).as_posix()}")

    for path in iter_example_files():
        rel = path.relative_to(ROOT).as_posix()
        text = read(path)
        for label, pattern in FORBIDDEN_PATTERNS.items():
            if pattern.search(text):
                fail(f"{label} in {rel}")

    main_text = read(MAIN_CPP)
    for label, pattern in REQUIRED_MAIN_PATTERNS.items():
        if not pattern.search(main_text):
            fail(f"missing {label} in {MAIN_CPP.relative_to(ROOT).as_posix()}")

    cmake_text = read(MAIN_CMAKE)
    for label, pattern in REQUIRED_MAIN_CMAKE.items():
        if not pattern.search(cmake_text):
            fail(f"main component missing {label}")
    for label, pattern in FORBIDDEN_CMAKE.items():
        if pattern.search(cmake_text):
            fail(f"forbidden Arduino dependency in IDF CMake: {label}")

    root_cmake_text = read(ROOT_CMAKE)
    for label, pattern in REQUIRED_ROOT_CMAKE.items():
        if not pattern.search(root_cmake_text):
            fail(f"root component missing {label}")
    for label, pattern in FORBIDDEN_ROOT_CMAKE.items():
        if pattern.search(root_cmake_text):
            fail(f"root component has forbidden dependency: {label}")

    library_version = str(json.loads(read(LIBRARY_JSON)).get("version", ""))
    manifest_text = read(IDF_COMPONENT_YML)
    manifest_version = re.search(r'^version:\s*"([^"]+)"\s*$', manifest_text, re.MULTILINE)
    if manifest_version is None:
        fail("idf_component.yml missing quoted version")
    if manifest_version.group(1) != library_version:
        fail(f"idf_component.yml version {manifest_version.group(1)} != library.json version {library_version}")
    if not re.search(r'^\s*idf:\s*">=5\.4"\s*$', manifest_text, re.MULTILINE):
        fail("idf_component.yml must require ESP-IDF >=5.4 for driver/i2c_master.h")

    print("IDF example contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
