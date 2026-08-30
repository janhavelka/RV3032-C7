#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
CODE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp", ".ino"}
FORBIDDEN_CORE_CALLS = ("millis", "micros", "delay", "delayMicroseconds", "yield")
FORBIDDEN_EXAMPLE_PATTERNS = {
    "atoi": r"\batoi\s*\(",
    "sscanf": r"\bsscanf\s*\(",
    "parseInt": r"\bparseInt\s*\(",
    "String.toInt": r"\.toInt\s*\(",
    "String.toFloat": r"\.toFloat\s*\(",
    "parallel tick owner": r"\bg_rtc\s*\.\s*tick\s*\(",
    "inferred job owner": r"\bg_rtc\s*\.\s*isJobBusy\s*\(",
    "direct EEPROM command": r"\bREG_EE_COMMAND\b",
}


def mask_non_code(text: str) -> str:
    """Replace comments and literals with spaces while preserving positions."""
    chars = list(text)
    i = 0
    state = "code"
    while i < len(chars):
        if state == "code":
            if text.startswith("//", i):
                chars[i] = chars[i + 1] = " "
                i += 2
                state = "line"
                continue
            if text.startswith("/*", i):
                chars[i] = chars[i + 1] = " "
                i += 2
                state = "block"
                continue
            if chars[i] in ('"', "'"):
                quote = chars[i]
                chars[i] = " "
                i += 1
                state = quote
                continue
        elif state == "line":
            if chars[i] == "\n":
                state = "code"
            else:
                chars[i] = " "
        elif state == "block":
            if text.startswith("*/", i):
                chars[i] = chars[i + 1] = " "
                i += 2
                state = "code"
                continue
            if chars[i] != "\n":
                chars[i] = " "
        else:
            if chars[i] == "\\" and i + 1 < len(chars):
                chars[i] = chars[i + 1] = " "
                i += 2
                continue
            if chars[i] == state:
                chars[i] = " "
                state = "code"
            elif chars[i] != "\n":
                chars[i] = " "
        i += 1
    return "".join(chars)


def matching_brace(code: str, opening: int) -> int | None:
    depth = 0
    for index in range(opening, len(code)):
        if code[index] == "{":
            depth += 1
        elif code[index] == "}":
            depth -= 1
            if depth == 0:
                return index + 1
    return None


def function_spans(code: str) -> list[tuple[str, int, int]]:
    spans: list[tuple[str, int, int]] = []
    for match in re.finditer(r"\bRV3032::([A-Za-z_]\w*)\s*\(", code):
        opening = code.find("{", match.end())
        if opening < 0:
            continue
        end = matching_brace(code, opening)
        if end is not None:
            spans.append((match.group(1), match.start(), end))
    return spans


def source_files(directory: str) -> list[pathlib.Path]:
    root = ROOT / directory
    return sorted(
        path for path in root.rglob("*")
        if path.is_file() and path.suffix.lower() in CODE_SUFFIXES
    )


def main() -> int:
    errors: list[str] = []

    for path in source_files("src") + source_files("include"):
        raw = path.read_text(encoding="utf-8", errors="replace")
        code = mask_non_code(raw)
        rel = path.relative_to(ROOT).as_posix()
        includes = re.finditer(
            r'^\s*#\s*include\s*[<"]Arduino\.h[>"]', raw, re.MULTILINE
        )
        if any(code[match.start()] == "#" or "#" in code[match.start():match.end()]
               for match in includes):
            errors.append(f"{rel}: core must not include Arduino.h")
        for call in FORBIDDEN_CORE_CALLS:
            if re.search(rf"\b{call}\s*\(", code):
                errors.append(f"{rel}: forbidden platform call {call}()")

    source_path = ROOT / "src/RV3032.cpp"
    source = source_path.read_text(encoding="utf-8", errors="replace")
    code = mask_non_code(source)
    spans = function_spans(code)
    for call in re.finditer(r"(?<!::)\b_updateHealth\s*\(", code):
        owner = next(
            (name for name, start, end in spans if start <= call.start() < end), None
        )
        tracked = owner == "finishTrackedTransferBefore" or (
            owner is not None and owner.startswith("_i2c") and "Tracked" in owner
        )
        if not tracked:
            line = source.count("\n", 0, call.start()) + 1
            errors.append(f"src/RV3032.cpp:{line}: health update outside tracked transport")

    untimed = re.compile(r"(?<![A-Za-z0-9_])(?:readRegs|writeRegs)\s*\(")
    engines = [
        (name, start, end) for name, start, end in spans
        if name in ("pollJob", "processEeprom") or name.startswith("processPersistent")
    ]
    found = {name for name, _, _ in engines}
    for required in ("pollJob", "processEeprom"):
        if required not in found:
            errors.append(f"src/RV3032.cpp: cooperative engine {required}() not found")
    if not any(name.startswith("processPersistent") for name in found):
        errors.append("src/RV3032.cpp: persistent cooperative engine not found")
    for engine, start, end in engines:
        span = (start, end)
        for call in untimed.finditer(code[span[0]:span[1]]):
            position = span[0] + call.start()
            line = source.count("\n", 0, position) + 1
            errors.append(f"src/RV3032.cpp:{line}: untimed register I/O in {engine}()")

    for path in source_files("examples"):
        code = mask_non_code(path.read_text(encoding="utf-8", errors="replace"))
        rel = path.relative_to(ROOT).as_posix()
        for label, pattern in FORBIDDEN_EXAMPLE_PATTERNS.items():
            if re.search(pattern, code):
                errors.append(f"{rel}: forbidden {label} path")

    if errors:
        print("Portability contract FAILED:")
        for error in errors:
            print(f"- {error}")
        return 1
    print("Portability contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
