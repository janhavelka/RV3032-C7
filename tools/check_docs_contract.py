#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys
import tarfile

ROOT = pathlib.Path(__file__).resolve().parents[1]

REQUIRED_SOURCE_FILES = [
    "README.md",
    "CHANGELOG.md",
    "Doxyfile",
    "library.json",
    "include/RV3032/Version.h",
    "docs/README.md",
    "docs/ARCHITECTURE.md",
    "docs/DEVICE_REFERENCE.md",
    "docs/IDF_PORT.md",
    "docs/reports/HIL_SUMMARY.md",
    "docs/reference-pdfs/RV-3032-C7_datasheet.pdf",
    "docs/reference-pdfs/RV-3032-C7_App-Manual.pdf",
]

REQUIRED_PACKAGE_FILES = [
    "README.md",
    "CHANGELOG.md",
    "Doxyfile",
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
    "examples/common/CliStyle.h",
    "examples/common/I2cScanner.h",
    "examples/common/Log.h",
    "docs/README.md",
    "docs/ARCHITECTURE.md",
    "docs/DEVICE_REFERENCE.md",
    "docs/IDF_PORT.md",
    "docs/reports/HIL_SUMMARY.md",
]

REQUIRED_EXPORT_EXCLUDES = [
    ".venv/**",
    "dist/**",
    "AGENTS.md",
    "docs/doxygen/**",
    "docs/extracted-md/**",
    "docs/prompts/**",
    "docs/reports/*.json",
    "docs/reports/*.pid",
    "docs/reports/*.txt",
    "docs/reports/*-runner.md",
    "idf_component.yml",
    "idf_component.yml.orig",
    "tmp/**",
    "docs/reference-pdfs/**",
    "docs/**/*.pdf",
]


def _read_library_json() -> dict:
    with (ROOT / "library.json").open("r", encoding="utf-8") as handle:
        return json.load(handle)


def _require_tokens(errors: list[str], rel: str, tokens: tuple[str, ...]) -> None:
    path = ROOT / rel
    if not path.is_file():
        return
    contents = path.read_text(encoding="utf-8", errors="replace")
    for token in tokens:
        if token not in contents:
            errors.append(f"maintained contract {rel} missing token: {token!r}")


def check_source() -> int:
    errors: list[str] = []
    for rel in REQUIRED_SOURCE_FILES:
        path = ROOT / rel
        if not path.is_file():
            errors.append(f"missing required source file: {rel}")
        elif path.stat().st_size == 0:
            errors.append(f"required source file is empty: {rel}")

    for rel in ("OPTION_A_PROPOSAL.txt", "build_output.txt"):
        if (ROOT / rel).exists():
            errors.append(f"obsolete root artifact remains: {rel}")

    forbidden_artifacts = [
        *ROOT.glob("docs/prompts/**/*.md"),
        *ROOT.glob("docs/extracted-md/**/*.md"),
        *ROOT.glob("docs/reports/20??-*.md"),
    ]
    for path in sorted(set(forbidden_artifacts)):
        errors.append(
            "completed workflow artifact remains: "
            f"{path.relative_to(ROOT).as_posix()}"
        )

    data = _read_library_json()
    version = data.get("version")
    if not isinstance(version, str) or not re.fullmatch(r"\d+\.\d+\.\d+", version):
        errors.append("library.json version must be a MAJOR.MINOR.PATCH string")
    excludes = data.get("export", {}).get("exclude", [])
    for pattern in REQUIRED_EXPORT_EXCLUDES:
        if pattern not in excludes:
            errors.append(f"library.json export.exclude missing {pattern!r}")

    doxyfile = (ROOT / "Doxyfile").read_text(encoding="utf-8", errors="replace")
    for setting in ("WARN_IF_DOC_ERROR", "WARN_AS_ERROR"):
        if not re.search(rf"(?m)^{setting}\s*=\s*YES\s*$", doxyfile):
            errors.append(f"Doxyfile must enable {setting}")
    if not re.search(r"(?m)^EXTRACT_ALL\s*=\s*YES\s*$", doxyfile):
        errors.append("Doxyfile must include the complete public API")
    if not re.search(r"(?m)^HAVE_DOT\s*=\s*NO\s*$", doxyfile):
        errors.append("Doxyfile must disable host-dependent Graphviz output")
    if re.search(r"(?m)^PROJECT_NUMBER\s*=\s*\S", doxyfile):
        errors.append("Doxyfile must not duplicate the library.json version")
    input_match = re.search(r"(?ms)^INPUT\s*=.*?(?=^\S)", doxyfile)
    public_inputs = input_match.group(0) if input_match else ""
    for historical_input in (
        "AGENTS.md",
        "docs/prompts",
        "docs/extracted-md",
        "docs/reports",
    ):
        if historical_input in public_inputs:
            errors.append(
                f"Doxyfile public INPUT includes non-API material: {historical_input}"
            )

    _require_tokens(
        errors,
        "docs/README.md",
        (
            "Maintained documents",
            "Vendor references",
            "Evidence policy",
            "Completed prompts and point-in-time implementation audits",
        ),
    )
    _require_tokens(
        errors,
        "README.md",
        (
            "begin()",
            "probe()",
            "ensurePrimaryCellConfiguration()",
            "ConfigurationJobReport",
            "Status tick(uint32_t nowMs)",
            "Wire example adapter",
            "CLI ownership",
            ".\\scripts\\pio.cmd test -e native",
            "docs/reports/HIL_SUMMARY.md",
        ),
    )
    _require_tokens(
        errors,
        "docs/ARCHITECTURE.md",
        (
            "Ownership and lifecycle",
            "Staged configuration and reconciliation",
            "Terminal bookkeeping has one owner",
            "readback-only reconciliation",
            "Active configuration and persistent EEPROM",
        ),
    )
    _require_tokens(
        errors,
        "docs/DEVICE_REFERENCE.md",
        (
            "Active calendar/control/register space: `0x00..0x2D`",
            "It is not BCD",
            "Status side effects",
            "Persistent content proof and access-state cleanup proof are independent",
            "Application Manual Rev. 1.3 pages 45",
        ),
    )
    _require_tokens(
        errors,
        "docs/IDF_PORT.md",
        (
            "Adapter boundary",
            "Status tick(uint32_t nowMs)",
            "Choose exactly one surface per owner-loop iteration.",
            "does not prove RV3032 silicon identity",
            "hard bound on the complete adapter callback",
        ),
    )
    _require_tokens(
        errors,
        "docs/reports/HIL_SUMMARY.md",
        (
            "Latest retained campaign",
            "157 PASS, 0 FAIL, 1 SKIP",
            "Battery retention",
            "Two-cycle configuration persistence",
            "Limits",
        ),
    )

    maintained_paths = [
        ROOT / "include/RV3032/RV3032.h",
        ROOT / "src/RV3032.cpp",
        ROOT / "examples/01_basic_bringup_cli/main.cpp",
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

    for path in maintained_paths[3:]:
        contents = path.read_text(encoding="utf-8", errors="replace")
        for token in (
            "BuildConfig.h",
            "BusDiag.h",
            "HealthDiag.h",
            "HealthView.h",
            "TransportAdapter.h",
            "DependencyVersions.h",
        ):
            if token in contents:
                errors.append(
                    f"removed cleanup surface {token!r} remains in "
                    f"{path.relative_to(ROOT)}"
                )
        for token in (
            r"\bTS_OVERWRITE_BIT\b",
            r"\bPMU_CLKOUT_DISABLE\b",
            r"\bVERSION_INT\b",
        ):
            if re.search(token, contents):
                errors.append(
                    f"removed compatibility alias {token!r} remains in "
                    f"{path.relative_to(ROOT)}"
                )

    public_header = (ROOT / "include/RV3032/RV3032.h").read_text(
        encoding="utf-8", errors="replace"
    )
    if "@class RV3032" in public_header:
        errors.append("public header contains ambiguous @class RV3032 directive")
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
        if (
            "either omitted flag is already set at the guard read" not in comment
            or "operation returns INVALID_PARAM without writing." not in comment
        ):
            errors.append(f"public Doxygen for {method} lacks omitted-flag guard contract")
        if (
            "An assertion" not in comment
            or "after the guard read cannot be preserved." not in comment
        ):
            errors.append(f"public Doxygen for {method} lacks post-guard race warning")

    readme_text = (ROOT / "README.md").read_text(encoding="utf-8", errors="replace")
    if readme_text.count("const uint32_t now = nowMs(nullptr);") < 5:
        errors.append("README polling snippets do not consistently sample current time")
    for forbidden in (
        "pollJob(nowMs,",
        "tick(nowMs)",
        ", nowMs, 4000",
        "python -m platformio",
    ):
        if forbidden in readme_text:
            errors.append(f"README retains invalid verification token: {forbidden!r}")

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
    if isinstance(version, str):
        expected_version_define = f'#define RV3032_VERSION_STRING "{version}"'
        if expected_version_define not in version_header:
            errors.append("generated Version.h does not match the library.json version")

    if errors:
        print("Docs source contract FAILED:")
        for error in errors:
            print(f"- {error}")
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
                if candidate.startswith(
                    (
                        "test/",
                        ".pio/",
                        ".venv/",
                        ".vscode/",
                        ".git/",
                        "AGENTS.md",
                        "dist/",
                        "tmp/",
                        "docs/prompts/",
                        "docs/extracted-md/",
                    )
                ):
                    forbidden.append(candidate)
                if (
                    candidate.startswith("docs/reports/")
                    and candidate != "docs/reports/HIL_SUMMARY.md"
                ):
                    forbidden.append(candidate)
                if candidate in (
                    "OPTION_A_PROPOSAL.txt",
                    "build_output.txt",
                    "idf_component.yml",
                    "idf_component.yml.orig",
                ):
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
        packaged_manifest_version = packaged_manifest.get("version")
        if (
            not isinstance(packaged_manifest_version, str)
            or not re.fullmatch(r"\d+\.\d+\.\d+", packaged_manifest_version)
        ):
            errors.append("packaged manifest version is not MAJOR.MINOR.PATCH")
        elif (
            f'#define RV3032_VERSION_STRING "{packaged_manifest_version}"'
            not in packaged_version
        ):
            errors.append("packaged Version.h does not match packaged manifest")
    except (KeyError, json.JSONDecodeError) as exc:
        errors.append(f"cannot validate packaged version agreement: {exc}")

    if errors:
        print("Docs package contract FAILED:")
        for error in errors:
            print(f"- {error}")
        return 1

    print("Docs package contract PASSED")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate documentation/package contract.")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("source", help="Check maintained docs and package metadata.")
    package_parser = sub.add_parser("package", help="Check a packed library archive.")
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
