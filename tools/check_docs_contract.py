#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import pathlib
import sys
import tarfile

ROOT = pathlib.Path(__file__).resolve().parents[1]

REQUIRED_SOURCE_FILES = [
    "README.md",
    "CHANGELOG.md",
    "library.json",
    "include/RV3032/Version.h",
    "docs/README.md",
    "docs/ARCHITECTURE.md",
    "docs/DEVICE_REFERENCE.md",
    "docs/IDF_PORT.md",
    "docs/reports/2026-07-13-v2.0.0-implementation.md",
    "docs/reference-pdfs/RV-3032-C7_datasheet.pdf",
    "docs/reference-pdfs/RV-3032-C7_App-Manual.pdf",
    "docs/extracted-md/00_document_inventory.md",
    "docs/extracted-md/01_chip_overview.md",
    "docs/extracted-md/02_pinout_and_signals.md",
    "docs/extracted-md/03_electrical_and_timing.md",
    "docs/extracted-md/04_protocol_commands_and_transactions.md",
    "docs/extracted-md/05_register_map.md",
    "docs/extracted-md/06_modes_interrupts_status_and_faults.md",
    "docs/extracted-md/07_initialization_reset_and_operational_notes.md",
    "docs/extracted-md/08_variant_differences_and_open_questions.md",
]

REQUIRED_PACKAGE_FILES = [
    "README.md",
    "CHANGELOG.md",
    "library.json",
    "include/RV3032/Version.h",
    "include/RV3032/RV3032.h",
    "include/RV3032/Config.h",
    "include/RV3032/Status.h",
    "include/RV3032/CommandTable.h",
    "src/RV3032.cpp",
    "docs/README.md",
    "docs/ARCHITECTURE.md",
    "docs/DEVICE_REFERENCE.md",
    "docs/IDF_PORT.md",
    "docs/reports/2026-07-13-v2.0.0-implementation.md",
    "docs/extracted-md/00_document_inventory.md",
    "docs/extracted-md/01_chip_overview.md",
    "docs/extracted-md/02_pinout_and_signals.md",
    "docs/extracted-md/03_electrical_and_timing.md",
    "docs/extracted-md/04_protocol_commands_and_transactions.md",
    "docs/extracted-md/05_register_map.md",
    "docs/extracted-md/06_modes_interrupts_status_and_faults.md",
    "docs/extracted-md/07_initialization_reset_and_operational_notes.md",
    "docs/extracted-md/08_variant_differences_and_open_questions.md",
]

REQUIRED_EXPORT_EXCLUDES = [
    "docs/reference-pdfs/**",
    "docs/**/*.pdf",
]


def _read_library_json() -> dict:
    with (ROOT / "library.json").open("r", encoding="utf-8") as handle:
        return json.load(handle)


def check_source() -> int:
    errors: list[str] = []
    for rel in REQUIRED_SOURCE_FILES:
        path = ROOT / rel
        if not path.is_file():
            errors.append(f"missing required source file: {rel}")
        elif path.stat().st_size == 0:
            errors.append(f"required source file is empty: {rel}")

    data = _read_library_json()
    excludes = data.get("export", {}).get("exclude", [])
    for pattern in REQUIRED_EXPORT_EXCLUDES:
        if pattern not in excludes:
            errors.append(f"library.json export.exclude missing {pattern!r}")

    report = ROOT / "docs/reports/2026-07-13-v2.0.0-implementation.md"
    if report.is_file():
        report_text = report.read_text(encoding="utf-8", errors="replace")
        for token in ("Vendor capability matrix", "Requirement-to-evidence matrix",
                      "Hardware-in-the-loop status is **NOT RUN**", "Worktree state"):
            if token not in report_text:
                errors.append(f"implementation report missing token: {token!r}")

    if errors:
        print("Docs source contract FAILED:")
        for err in errors:
            print(f"- {err}")
        return 1

    print("Docs source contract PASSED")
    return 0


def _package_member_candidates(member_name: str) -> set[str]:
    normalized = member_name.replace("\\", "/").strip("/")
    candidates = {normalized}
    parts = normalized.split("/")
    if len(parts) > 1:
        candidates.add("/".join(parts[1:]))
    return candidates


def check_package(archive: pathlib.Path) -> int:
    errors: list[str] = []
    if not archive.is_file():
        print(f"Docs package contract FAILED:\n- archive not found: {archive}")
        return 1

    seen: set[str] = set()
    forbidden: list[str] = []
    with tarfile.open(archive, "r:*") as tar:
        for member in tar.getmembers():
            if not member.isfile():
                continue
            for candidate in _package_member_candidates(member.name):
                seen.add(candidate)
                if candidate.startswith("docs/reference-pdfs/") or candidate.endswith(".pdf"):
                    forbidden.append(candidate)

    for rel in REQUIRED_PACKAGE_FILES:
        if rel not in seen:
            errors.append(f"missing required package file: {rel}")

    for rel in sorted(set(forbidden)):
        errors.append(f"forbidden package file included: {rel}")

    if errors:
        print("Docs package contract FAILED:")
        for err in errors:
            print(f"- {err}")
        return 1

    print("Docs package contract PASSED")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate documentation/package contract.")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("source", help="Check required docs and package metadata in the source tree.")
    package_parser = sub.add_parser("package", help="Check required and forbidden files in a packed archive.")
    package_parser.add_argument("archive", type=pathlib.Path)
    args = parser.parse_args()

    if args.command == "source":
        return check_source()
    if args.command == "package":
        return check_package(args.archive)
    parser.error("unknown command")
    return 2


if __name__ == "__main__":
    sys.exit(main())
