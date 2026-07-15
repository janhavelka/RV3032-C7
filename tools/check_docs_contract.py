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
    "docs/reports/2026-07-14-full-library-functional-audit.md",
    "docs/reports/2026-07-15-functional-hardening-closure-audit.md",
    "docs/reports/2026-07-13-v2.0.0-implementation.md",
    "docs/reports/2026-07-14-tunnelmonitor-integration-readiness.md",
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
    "platformio.ini",
    "examples/01_basic_bringup_cli/main.cpp",
    "examples/common/I2cTransport.h",
    "examples/common/CliShell.h",
    "examples/common/CommandHandler.h",
    "examples/common/BoardConfig.h",
    "examples/common/BuildConfig.h",
    "examples/common/BusDiag.h",
    "examples/common/CliStyle.h",
    "examples/common/HealthDiag.h",
    "examples/common/HealthView.h",
    "examples/common/I2cScanner.h",
    "examples/common/Log.h",
    "examples/common/TransportAdapter.h",
    "docs/README.md",
    "docs/ARCHITECTURE.md",
    "docs/DEVICE_REFERENCE.md",
    "docs/IDF_PORT.md",
    "docs/reports/2026-07-14-full-library-functional-audit.md",
    "docs/reports/2026-07-15-functional-hardening-closure-audit.md",
    "docs/reports/2026-07-13-v2.0.0-implementation.md",
    "docs/reports/2026-07-14-tunnelmonitor-integration-readiness.md",
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
    ".venv/**",
    "OPTION_A_PROPOSAL.txt",
    "build_output.txt",
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
    if data.get("version") != "3.0.0":
        errors.append("library.json version must be 3.0.0 for v3 hardening")
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

    readiness_report = (
        ROOT / "docs/reports/2026-07-14-tunnelmonitor-integration-readiness.md"
    )
    if readiness_report.is_file():
        readiness_text = readiness_report.read_text(
            encoding="utf-8", errors="replace"
        )
        for token in (
            "Four closed compatibility gaps",
            "Requirement-to-evidence matrix",
            "Physical HIL status is **NOT RUN**",
            "Final worktree state",
        ):
            if token not in readiness_text:
                errors.append(
                    f"readiness report missing token: {token!r}"
                )

    docs_index = ROOT / "docs/README.md"
    if docs_index.is_file():
        docs_text = docs_index.read_text(encoding="utf-8", errors="replace")
        for token in (
            "Active Functional-Hardening Prompt Suite 06",
            "2026-07-14-full-library-functional-audit.md",
            "2026-07-15-functional-hardening-closure-audit.md",
            "historical where they",
        ):
            if token not in docs_text:
                errors.append(f"docs index missing active-hardening token: {token!r}")

    closure_report = (
        ROOT / "docs/reports/2026-07-15-functional-hardening-closure-audit.md"
    )
    if closure_report.is_file():
        closure_text = closure_report.read_text(
            encoding="utf-8", errors="replace"
        )
        for token in (
            "Requirement-to-evidence closure matrix",
            "109/109",
            "Physical HIL status is **NOT RUN**",
            "Final worktree state",
        ):
            if token not in closure_text:
                errors.append(
                    f"closure report missing token: {token!r}"
                )

    maintained_contract_tokens = {
        "README.md": (
            "ConfigurationJobReport",
            "4 * i2cTimeoutMs + activationMs + 1",
            "UIE=0",
            "Status tick(uint32_t nowMs)",
            "operationStatus",
            "cleanupStatus",
            "Wire example adapter",
            "CLI ownership",
            "const uint32_t now = nowMs(nullptr);",
            "chip identity",
            "wrap from `UINT32_MAX` to zero",
        ),
        "docs/ARCHITECTURE.md": (
            "Staged configuration and reconciliation",
            "SAFE_DISABLED_VERIFIED",
            "readback-only reconciliation",
            "TIE=0",
            "AIE=0",
            "EIE=0",
            "BSIE=0",
            "short staging write",
            "PendingOperation",
            "cannot prove RV3032",
        ),
        "docs/DEVICE_REFERENCE.md": (
            "UF-polling-only mode",
            "2 ms activation not-before boundary",
            "Persistent content proof and access-state cleanup proof are independent",
            "It is not BCD",
            "An assertion after the guard read cannot be",
        ),
        "docs/IDF_PORT.md": (
            "Status tick(uint32_t nowMs)",
            "operation/cleanup evidence",
            "TIE/AIE/EIE/BSIE",
            "does not prove RV3032 silicon identity",
            "hard bound on the complete adapter callback",
            "const uint32_t now = idfNowMs(nullptr);",
            "if (rtc.isJobBusy())",
            "pollStatus = rtc.pollJob(now, 1, used);",
            "pollStatus = rtc.pollEeprom(now, 1, used);",
            "Choose exactly one surface per owner-loop iteration.",
        ),
    }
    for rel, tokens in maintained_contract_tokens.items():
        path = ROOT / rel
        if not path.is_file():
            continue
        contents = path.read_text(encoding="utf-8", errors="replace")
        for token in tokens:
            if token not in contents:
                errors.append(
                    f"maintained contract {rel} missing token: {token!r}"
                )

    maintained_paths = [
        ROOT / "include/RV3032/RV3032.h",
        ROOT / "src/RV3032.cpp",
        ROOT / "examples/01_basic_bringup_cli/main.cpp",
        ROOT / "examples/common/HealthDiag.h",
        ROOT / "examples/common/HealthView.h",
        ROOT / "README.md",
        ROOT / "docs/README.md",
        ROOT / "docs/ARCHITECTURE.md",
        ROOT / "docs/DEVICE_REFERENCE.md",
        ROOT / "docs/IDF_PORT.md",
    ]
    for path in maintained_paths:
        contents = path.read_text(encoding="utf-8", errors="replace")
        if "isOnline" in contents:
            errors.append(f"removed isOnline surface remains in {path.relative_to(ROOT)}")
        if "presence and identity" in contents:
            errors.append(
                f"false presence/identity claim remains in {path.relative_to(ROOT)}"
            )

    public_header = (ROOT / "include/RV3032/RV3032.h").read_text(
        encoding="utf-8", errors="replace"
    )
    warning = "Any Status-register write clears THF and TLF in silicon."
    for method in (
        "clearAlarmFlag",
        "clearTimerFlag",
        "clearPeriodicUpdateFlag",
        "clearEventFlag",
        "clearPowerOnResetFlag",
        "clearVoltageLowFlag",
    ):
        declaration = f"Status {method}("
        declaration_index = public_header.find(declaration)
        comment_index = public_header.rfind("/**", 0, declaration_index)
        if declaration_index < 0 or comment_index < 0:
            errors.append(f"public Doxygen owner missing for {method}")
            continue
        comment = public_header[comment_index:declaration_index]
        if warning not in comment:
            errors.append(f"public Doxygen for {method} lacks Status-write warning")
        if ("either omitted flag is already set at the guard read" not in comment or
                "operation returns INVALID_PARAM without writing." not in comment):
            errors.append(f"public Doxygen for {method} lacks omitted-flag guard contract")
        if ("An assertion" not in comment or
                "after the guard read cannot be preserved." not in comment):
            errors.append(f"public Doxygen for {method} lacks post-guard race warning")

    readme_text = (ROOT / "README.md").read_text(
        encoding="utf-8", errors="replace"
    )
    if readme_text.count("const uint32_t now = nowMs(nullptr);") < 5:
        errors.append("README polling snippets do not consistently sample current time")
    for forbidden in ("pollJob(nowMs,", "tick(nowMs)", ", nowMs, 4000"):
        if forbidden in readme_text:
            errors.append(f"README retains uncompilable polling token: {forbidden!r}")

    idf_text = (ROOT / "docs/IDF_PORT.md").read_text(
        encoding="utf-8", errors="replace"
    )
    if "rtc.pollJob(idfNowMs(nullptr), 1, used);" in idf_text:
        errors.append("IDF owner loop retains unsampled ordinary-job poll")
    if "rtc.pollEeprom(idfNowMs(nullptr), 1, used);" in idf_text:
        errors.append("IDF owner loop retains unsampled EEPROM poll")

    version_header = (ROOT / "include/RV3032/Version.h").read_text(
        encoding="utf-8", errors="replace"
    )
    if '#define RV3032_VERSION_STRING "3.0.0"' not in version_header:
        errors.append("generated Version.h does not expose 3.0.0")

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
    contents_by_name: dict[str, bytes] = {}
    with tarfile.open(archive, "r:*") as tar:
        for member in tar.getmembers():
            if not member.isfile():
                continue
            for candidate in _package_member_candidates(member.name):
                seen.add(candidate)
                if candidate in ("library.json", "include/RV3032/Version.h"):
                    extracted = tar.extractfile(member)
                    if extracted is not None:
                        contents_by_name[candidate] = extracted.read()
                if candidate.startswith("docs/reference-pdfs/") or candidate.endswith(".pdf"):
                    forbidden.append(candidate)
                if candidate.startswith((
                    "test/", ".pio/", ".venv/", ".vscode/", ".git/",
                    "dist/", "tmp/",
                )):
                    forbidden.append(candidate)
                if candidate in ("OPTION_A_PROPOSAL.txt", "build_output.txt"):
                    forbidden.append(candidate)

    for rel in REQUIRED_PACKAGE_FILES:
        if rel not in seen:
            errors.append(f"missing required package file: {rel}")

    for rel in sorted(set(forbidden)):
        errors.append(f"forbidden package file included: {rel}")

    try:
        packaged_manifest = json.loads(contents_by_name["library.json"].decode("utf-8"))
        packaged_version = contents_by_name["include/RV3032/Version.h"].decode(
            "utf-8", errors="replace"
        )
        if packaged_manifest.get("version") != "3.0.0":
            errors.append("packaged manifest version is not 3.0.0")
        if '#define RV3032_VERSION_STRING "3.0.0"' not in packaged_version:
            errors.append("packaged Version.h does not match manifest 3.0.0")
    except (KeyError, json.JSONDecodeError) as exc:
        errors.append(f"cannot validate packaged version agreement: {exc}")

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
