#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys
from typing import Dict

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCAN_DIRS = ("src", "include")
VALID_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp"}

FORBIDDEN_CODE_PATTERNS = {
    "millis": re.compile(r"\bmillis\s*\("),
    "micros": re.compile(r"\bmicros\s*\("),
    "delay": re.compile(r"\bdelay\s*\("),
    "delayMicroseconds": re.compile(r"\bdelayMicroseconds\s*\("),
    "yield": re.compile(r"\byield\s*\("),
    "Serial": re.compile(r"\bSerial\b"),
    "String": re.compile(r"\bString\b"),
    "esp_timer_get_time": re.compile(r"\besp_timer_get_time\s*\("),
    "ESP_LOG": re.compile(r"\bESP_LOG[A-Z]*\s*\("),
    "std::vector": re.compile(r"\bstd::vector\b"),
    "new": re.compile(r"\bnew\b"),
    "malloc": re.compile(r"\bmalloc\s*\("),
    "calloc": re.compile(r"\bcalloc\s*\("),
    "realloc": re.compile(r"\brealloc\s*\("),
    "free": re.compile(r"\bfree\s*\("),
}

FORBIDDEN_RAW_PATTERNS = {
    "Arduino token": re.compile(r"\bArduino\b"),
    "Wire token": re.compile(r"\bWire\b"),
    "TwoWire token": re.compile(r"\bTwoWire\b"),
    "Serial token": re.compile(r"\bSerial\b"),
    "FreeRTOS token": re.compile(r"\bFreeRTOS\b"),
    "String token": re.compile(r"\bString\b"),
    "millis call": re.compile(r"\bmillis\s*\("),
    "micros call": re.compile(r"\bmicros\s*\("),
    "delay call": re.compile(r"\bdelay\s*\("),
    "yield call": re.compile(r"\byield\s*\("),
    "esp_timer token": re.compile(r"\besp_timer\b"),
    "ESP_LOG token": re.compile(r"\bESP_LOG[A-Z]*\b"),
}

FORBIDDEN_INCLUDE_RE = re.compile(
    r'^\s*#\s*include\s*[<"]'
    r'(Arduino\.h|Wire\.h|freertos/[^>"]+|FreeRTOS\.h|driver/i2c\.h|'
    r'esp_timer\.h|esp_log\.h|esp_err\.h)'
    r'[>"]',
    re.MULTILINE,
)
BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
LINE_COMMENT_RE = re.compile(r"//[^\n]*")
STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')

ALLOWED_CODE_COUNTS: Dict[str, Dict[str, int]] = {}
ALLOWED_RAW_COUNTS: Dict[str, Dict[str, int]] = {}
ALLOWED_INCLUDE_COUNTS: Dict[str, int] = {}


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
    observed_code: Dict[str, Dict[str, int]] = {}
    observed_raw: Dict[str, Dict[str, int]] = {}
    observed_includes: Dict[str, int] = {}

    for path in collect_sources():
        rel = path.relative_to(ROOT).as_posix()
        raw = path.read_text(encoding="utf-8", errors="replace")
        code = strip_non_code(raw)

        code_counts: Dict[str, int] = {}
        for call_name, pattern in FORBIDDEN_CODE_PATTERNS.items():
            count = len(pattern.findall(code))
            if count > 0:
                code_counts[call_name] = count
        if code_counts:
            observed_code[rel] = code_counts

        raw_counts: Dict[str, int] = {}
        for token_name, pattern in FORBIDDEN_RAW_PATTERNS.items():
            count = len(pattern.findall(raw))
            if count > 0:
                raw_counts[token_name] = count
        if raw_counts:
            observed_raw[rel] = raw_counts

        include_count = len(FORBIDDEN_INCLUDE_RE.findall(raw))
        if include_count > 0:
            observed_includes[rel] = include_count

    errors: list[str] = []

    for rel, counts in observed_code.items():
        if rel not in ALLOWED_CODE_COUNTS:
            errors.append(f"forbidden core code patterns in unexpected file: {rel} -> {counts}")
            continue
        expected = ALLOWED_CODE_COUNTS[rel]
        for call_name, count in counts.items():
            exp = expected.get(call_name, 0)
            if count != exp:
                errors.append(
                    f"core code pattern count mismatch in {rel}: {call_name} observed={count}, expected={exp}"
                )

    for rel, expected in ALLOWED_CODE_COUNTS.items():
        observed = observed_code.get(rel, {})
        for call_name, exp in expected.items():
            obs = observed.get(call_name, 0)
            if obs != exp:
                errors.append(
                    f"core code pattern count mismatch in {rel}: {call_name} observed={obs}, expected={exp}"
                )
        unexpected_calls = set(observed.keys()) - set(expected.keys())
        if unexpected_calls:
            errors.append(f"unexpected core code pattern types in {rel}: {sorted(unexpected_calls)}")

    for rel, counts in observed_raw.items():
        if rel not in ALLOWED_RAW_COUNTS:
            errors.append(f"forbidden core raw tokens in unexpected file: {rel} -> {counts}")
            continue
        expected = ALLOWED_RAW_COUNTS[rel]
        for token_name, count in counts.items():
            exp = expected.get(token_name, 0)
            if count != exp:
                errors.append(
                    f"core raw token count mismatch in {rel}: {token_name} observed={count}, expected={exp}"
                )

    for rel, expected in ALLOWED_RAW_COUNTS.items():
        observed = observed_raw.get(rel, {})
        for token_name, exp in expected.items():
            obs = observed.get(token_name, 0)
            if obs != exp:
                errors.append(
                    f"core raw token count mismatch in {rel}: {token_name} observed={obs}, expected={exp}"
                )
        unexpected_tokens = set(observed.keys()) - set(expected.keys())
        if unexpected_tokens:
            errors.append(f"unexpected core raw token types in {rel}: {sorted(unexpected_tokens)}")

    for rel, count in observed_includes.items():
        exp = ALLOWED_INCLUDE_COUNTS.get(rel, 0)
        if count != exp:
            errors.append(
                f"Arduino include count mismatch in {rel}: observed={count}, expected={exp}"
            )

    for rel, exp in ALLOWED_INCLUDE_COUNTS.items():
        obs = observed_includes.get(rel, 0)
        if obs != exp:
            errors.append(
                f"Arduino include count mismatch in {rel}: observed={obs}, expected={exp}"
            )

    if errors:
        print("Core timing/framework guard FAILED:")
        for err in errors:
            print(f"- {err}")
        return 1

    print("Core timing/framework guard PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
