#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
CODE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp", ".ino"}
FORBIDDEN_CORE_CALLS = ("millis", "micros", "delay", "delayMicroseconds", "yield")
FORBIDDEN_PARSER_PATTERNS = {
    "atoi": r"\batoi\s*\(",
    "sscanf": r"\bsscanf\s*\(",
    "strtol": r"\bstrtol\s*\(",
    "sprintf": r"\bsprintf\s*\(",
}
FORBIDDEN_EXAMPLE_PATTERNS = {
    **FORBIDDEN_PARSER_PATTERNS,
    "parseInt": r"\bparseInt\s*\(",
    "String.toInt": r"\.toInt\s*\(",
    "String.toFloat": r"\.toFloat\s*\(",
    "parallel tick owner": r"\bg_rtc\s*\.\s*tick\s*\(",
    "inferred job owner": r"\bg_rtc\s*\.\s*isJobBusy\s*\(",
    "direct EEPROM command": r"\bREG_EE_COMMAND\b",
}
HEALTH_UPDATE_OWNERS = {
    "_i2cWriteReadTracked", "_i2cWriteTracked",
    "_i2cWriteReadTrackedTimeout", "_i2cWriteTrackedTimeout",
    "_i2cWriteReadPresenceTracked", "finishTrackedTransferBefore",
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


def core_source_errors(raw: str, rel: str) -> list[str]:
    errors: list[str] = []
    code = mask_non_code(raw)
    for header in ("Arduino.h", "cstdio", "stdio.h"):
        includes = re.finditer(
            rf'^\s*#\s*include\s*[<"]{re.escape(header)}[>"]', raw, re.MULTILINE)
        if any("#" in code[match.start():match.end()] for match in includes):
            errors.append(f"{rel}: core must not include {header}")
    for call in FORBIDDEN_CORE_CALLS:
        if re.search(rf"\b{call}\s*\(", code):
            errors.append(f"{rel}: forbidden platform call {call}()")
    for label, pattern in FORBIDDEN_PARSER_PATTERNS.items():
        if re.search(pattern, code):
            errors.append(f"{rel}: forbidden {label} path")
    return errors


def cooperative_contract_errors(source: str) -> list[str]:
    errors: list[str] = []
    code = mask_non_code(source)
    spans = function_spans(code)
    for call in re.finditer(r"(?<!::)\b_updateHealth\s*\(", code):
        owner = next(
            (name for name, start, end in spans if start <= call.start() < end), None
        )
        if owner not in HEALTH_UPDATE_OWNERS:
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
    members: dict[str, list[tuple[int, int]]] = {}
    for name, start, end in spans:
        members.setdefault(name, []).append((code.index("{", start) + 1, end))
    pending = list(found)
    reachable: set[str] = set()
    while pending:
        name = pending.pop()
        if name in reachable:
            continue
        reachable.add(name)
        for start, end in members[name]:
            calls = set(re.findall(r"\b([A-Za-z_]\w*)\s*\(", code[start:end]))
            pending.extend(calls.intersection(members).difference(reachable))
    for engine in sorted(reachable):
        for start, end in members[engine]:
            for call in untimed.finditer(code[start:end]):
                position = start + call.start()
                line = source.count("\n", 0, position) + 1
                errors.append(
                    f"src/RV3032.cpp:{line}: untimed register I/O in {engine}()")
    return errors


def main() -> int:
    errors: list[str] = []
    for path in source_files("src") + source_files("include"):
        errors.extend(core_source_errors(
            path.read_text(encoding="utf-8", errors="replace"),
            path.relative_to(ROOT).as_posix()))
    errors.extend(cooperative_contract_errors(
        (ROOT / "src/RV3032.cpp").read_text(encoding="utf-8")))

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
