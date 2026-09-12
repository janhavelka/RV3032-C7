#!/usr/bin/env python3
from __future__ import annotations

import argparse
import fnmatch
import json
import pathlib
import re
import sys
import tarfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
REQUIRED_SOURCE_FILES = (
    "README.md",
    "CHANGELOG.md",
    "Doxyfile",
    "library.json",
    "include/RV3032/Version.h",
    "docs/README.md",
    "docs/ARCHITECTURE.md",
    "docs/DEVICE_REFERENCE.md",
    "docs/IDF_PORT.md",
    "docs/VERIFICATION.md",
    "docs/reference-pdfs/RV-3032-C7_datasheet.pdf",
    "docs/reference-pdfs/RV-3032-C7_App-Manual.pdf",
)
REQUIRED_PACKAGE_FILES = (
    "README.md", "CHANGELOG.md", "Doxyfile", "library.json", "platformio.ini",
    "include/RV3032/Version.h", "include/RV3032/RV3032.h",
    "include/RV3032/Config.h", "include/RV3032/Status.h",
    "include/RV3032/CommandTable.h", "src/RV3032.cpp",
    "examples/01_basic_bringup_cli/main.cpp",
    "examples/common/I2cTransport.h", "examples/common/CliShell.h",
    "examples/common/CommandHandler.h", "examples/common/BoardConfig.h",
    "examples/common/CliStyle.h", "examples/common/I2cScanner.h",
    "examples/common/Log.h", "docs/README.md", "docs/ARCHITECTURE.md",
    "docs/DEVICE_REFERENCE.md", "docs/IDF_PORT.md",
    "docs/VERIFICATION.md",
)
REQUIRED_EXPORT_EXCLUDES = (
    ".github/**", ".pio/**", ".venv/**", ".vscode/**", "dist/**", "AGENTS.md",
    "docs/CODE_AUDIT*.md", "docs/doxygen/**", "docs/extracted-md/**",
    "docs/prompts/**", "docs/reports/**", "docs/reference-pdfs/**",
    "docs/**/*.pdf", "idf_component.yml", "idf_component.yml.orig", "test/**",
    "tmp/**", "*.tar.gz", "*.tgz", "*.zip",
)
STATUS_CLEAR_METHODS = (
    "clearAlarmFlag", "clearTimerFlag", "clearPeriodicUpdateFlag",
    "clearEventFlag", "clearPowerOnResetFlag", "clearVoltageLowFlag",
)


def library_manifest() -> dict:
    return json.loads((ROOT / "library.json").read_text(encoding="utf-8"))


def preceding_doxygen(header: str, declaration: int) -> str:
    start = header.rfind("/**", 0, declaration)
    end = header.rfind("*/", 0, declaration)
    return "" if start < 0 or end < start else header[start:end + 2]


def source_check() -> int:
    errors: list[str] = []
    for rel in REQUIRED_SOURCE_FILES:
        path = ROOT / rel
        if not path.is_file() or path.stat().st_size == 0:
            errors.append(f"missing or empty required source file: {rel}")

    artifacts = [
        *ROOT.glob("docs/CODE_AUDIT*.md"),
        *ROOT.glob("docs/prompts/**/*"),
        *ROOT.glob("docs/extracted-md/**/*"),
        *ROOT.glob("docs/reports/**/*"),
    ]
    for path in sorted({path for path in artifacts if path.is_file()}):
        errors.append(f"completed workflow artifact remains: {path.relative_to(ROOT).as_posix()}")

    manifest = library_manifest()
    excludes = manifest.get("export", {}).get("exclude", [])
    if not isinstance(excludes, list):
        errors.append("library.json export.exclude must be a list")
        excludes = []
    for pattern in REQUIRED_EXPORT_EXCLUDES:
        if pattern not in excludes:
            errors.append(f"library.json export.exclude missing {pattern!r}")
    if "AUDIT.md" in excludes:
        errors.append("library.json retains stale AUDIT.md exclude")
    for probe in (
        ".github/workflows/ci.yml",
        "docs/doxygen/index.html",
        "RV3032-C7.tar.gz",
        "RV3032-C7.tgz",
        "RV3032-C7.zip",
    ):
        if not forbidden_package_path(probe):
            errors.append(f"package path policy permits excluded probe: {probe}")

    doxyfile = (ROOT / "Doxyfile").read_text(encoding="utf-8", errors="replace")
    for setting, value in (
        ("WARN_IF_DOC_ERROR", "YES"), ("WARN_AS_ERROR", "YES"),
        ("EXTRACT_ALL", "YES"), ("HAVE_DOT", "NO"),
    ):
        if re.search(rf"(?m)^{setting}\s*=\s*{value}\s*$", doxyfile) is None:
            errors.append(f"Doxyfile must set {setting} = {value}")
    if re.search(r"(?m)^PROJECT_NUMBER\s*=\s*\S", doxyfile):
        errors.append("Doxyfile must not duplicate the manifest version")
    input_block = re.search(r"(?ms)^INPUT\s*=.*?(?=^[A-Z][A-Z0-9_]*\s*=|\Z)", doxyfile)
    if input_block is None:
        errors.append("Doxyfile INPUT is missing")
    else:
        for forbidden in ("AGENTS.md", "docs/prompts", "docs/extracted-md", "docs/reports"):
            if forbidden in input_block.group(0):
                errors.append(f"Doxyfile INPUT includes internal artifact path {forbidden!r}")

    runner = (ROOT / "tools/hil_cli_runner.py").read_text(encoding="utf-8")
    for token in ("--authorization-c0-write", "CONFIRM-POSSIBLE-C0-WRITE"):
        if token not in runner:
            errors.append(f"destructive HIL authorization token missing: {token!r}")

    header = (ROOT / "include/RV3032/RV3032.h").read_text(encoding="utf-8")
    safety_phrase = "Any Status-register write clears THF and TLF"
    for method in STATUS_CLEAR_METHODS:
        match = re.search(rf"\bStatus\s+{method}\s*\(", header)
        comment = "" if match is None else preceding_doxygen(header, match.start())
        if match is None or safety_phrase not in comment:
            errors.append(f"{method} Doxygen lacks the Status-write safety warning")

    if errors:
        print("Package source contract FAILED:")
        for error in errors:
            print(f"- {error}")
        return 1
    print("Package source contract PASSED")
    return 0


def member_candidates(name: str) -> set[str]:
    normalized = name.replace("\\", "/").strip("/")
    candidates = {normalized}
    if "/" in normalized:
        candidates.add(normalized.split("/", 1)[1])
    return candidates


def forbidden_package_path(path: str) -> bool:
    if any(fnmatch.fnmatchcase(path, pattern)
           for pattern in REQUIRED_EXPORT_EXCLUDES):
        return True
    # Defense in depth for repository metadata and broad generated artifacts.
    if path.endswith(".pdf") or fnmatch.fnmatch(path, "docs/CODE_AUDIT*.md"):
        return True
    if path.startswith((
        "test/", ".pio/", ".venv/", ".vscode/", ".git/", "dist/", "tmp/",
        "docs/prompts/", "docs/extracted-md/", "docs/reports/", "docs/reference-pdfs/",
    )):
        return True
    if path == "AGENTS.md":
        return True
    return path in ("idf_component.yml", "idf_component.yml.orig")


def package_check(archive: pathlib.Path) -> int:
    if not archive.is_file():
        print(f"Package archive contract FAILED:\n- archive not found: {archive}")
        return 1

    errors: list[str] = []
    seen: set[str] = set()
    contents: dict[str, bytes] = {}
    with tarfile.open(archive, "r:*") as package:
        for member in package.getmembers():
            normalized = member.name.replace("\\", "/")
            if normalized.startswith("/") or ".." in pathlib.PurePosixPath(normalized).parts:
                errors.append(f"unsafe package path: {member.name}")
                continue
            if not member.isfile():
                continue
            for candidate in member_candidates(member.name):
                seen.add(candidate)
                if forbidden_package_path(candidate):
                    errors.append(f"forbidden package file included: {candidate}")
                if candidate in ("library.json", "include/RV3032/Version.h"):
                    handle = package.extractfile(member)
                    if handle is not None:
                        contents[candidate] = handle.read()

    for rel in REQUIRED_PACKAGE_FILES:
        if rel not in seen:
            errors.append(f"missing required package file: {rel}")
    try:
        manifest = json.loads(contents["library.json"].decode("utf-8"))
        version = manifest.get("version")
        version_header = contents["include/RV3032/Version.h"].decode(
            "utf-8", errors="replace"
        )
        if not isinstance(version, str) or re.fullmatch(r"\d+\.\d+\.\d+", version) is None:
            errors.append("packaged manifest version is not MAJOR.MINOR.PATCH")
        elif f'#define RV3032_VERSION_STRING "{version}"' not in version_header:
            errors.append("packaged Version.h does not match packaged manifest")
    except (KeyError, json.JSONDecodeError) as exc:
        errors.append(f"cannot validate packaged version agreement: {exc}")

    if errors:
        print("Package archive contract FAILED:")
        for error in sorted(set(errors)):
            print(f"- {error}")
        return 1
    print("Package archive contract PASSED")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate source/package contracts.")
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("source", help="Validate maintained source metadata.")
    package_parser = subparsers.add_parser("package", help="Validate a packed archive.")
    package_parser.add_argument("archive", type=pathlib.Path)
    args = parser.parse_args()
    return source_check() if args.command == "source" else package_check(args.archive)


if __name__ == "__main__":
    sys.exit(main())
