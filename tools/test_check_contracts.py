"""Mutation probes for the maintained source/ABI contract checks."""
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import re
import tarfile
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

import check_abi as abi
import check_package as package
import check_portability as portability


class PortabilityChecks(unittest.TestCase):
    def test_core_stdio_and_parser_regressions(self):
        for text in ('#include <cstdio>', '#include "stdio.h"',
                     'std::sscanf(x, y);', 'atoi(x);', 'strtol(x, y, 10);',
                     'sprintf(x, y);'):
            with self.subTest(text=text):
                self.assertTrue(portability.core_source_errors(text, "src/probe.cpp"))
        self.assertFalse(portability.core_source_errors(
            '// #include <cstdio>\nconst char* text = "sscanf(x)";', "include/probe.h"))

    def test_cooperative_helper_cannot_hide_untimed_io(self):
        source = '''
Status RV3032::pollJob() { return helper(); }
Status RV3032::helper() { return nested(); }
Status RV3032::nested() { helper(); return readRegs(0, data, 1); }
Status RV3032::processEeprom() { return processPersistent(); }
Status RV3032::processPersistent() { return Ok(); }
'''
        errors = portability.cooperative_contract_errors(source)
        self.assertTrue(any("untimed register I/O in nested()" in error for error in errors))
        self.assertFalse(portability.cooperative_contract_errors(
            source.replace("readRegs(", "readRegsBefore(")))

    def test_tracked_sounding_name_is_not_a_health_owner(self):
        source = '''
Status RV3032::pollJob() { return Ok(); }
Status RV3032::processEeprom() { return Ok(); }
Status RV3032::processPersistent() { return Ok(); }
Status RV3032::_i2cFakeTrackedOwner() { return _updateHealth(st); }
'''
        self.assertTrue(any("health update outside" in error for error in
                            portability.cooperative_contract_errors(source)))
        for owner in portability.HEALTH_UPDATE_OWNERS:
            self.assertFalse(portability.cooperative_contract_errors(
                source.replace("_i2cFakeTrackedOwner", owner)))

    def test_qualified_health_calls_are_not_definitions(self):
        source = '''
Status RV3032::pollJob() { RV3032::_updateHealth(st); return Ok(); }
Status RV3032::processEeprom() { return Ok(); }
Status RV3032::processPersistent() { return Ok(); }
Status RV3032::_updateHealth(const Status& st) { return st; }
'''
        errors = portability.cooperative_contract_errors(source)
        self.assertEqual(1, len(errors))
        self.assertIn("health update outside", errors[0])
        self.assertFalse(portability.cooperative_contract_errors(
            source.replace("RV3032::_updateHealth(st);", "")))

    def test_inline_and_separate_file_members_are_reachable(self):
        sources = {
            "src/RV3032.cpp": '''
Status RV3032::pollJob() { return helper(); }
Status RV3032::processEeprom() { return Ok(); }
Status RV3032::processPersistent() { return Ok(); }
''',
            "include/RV3032/RV3032.h": '''
class RV3032 {
  Status helper() { return nested(); }
  Status _updateHealth(const Status& st);
};
''',
            "src/helper.cpp": '''
Status RV3032::nested() { return writeRegs(0, data, 1); }
''',
        }
        errors = portability.cooperative_contract_errors(sources)
        self.assertEqual(1, len(errors))
        self.assertIn("src/helper.cpp:2: untimed register I/O in nested()", errors[0])
        sources["src/helper.cpp"] = sources["src/helper.cpp"].replace(
            "writeRegs(", "writeRegsBefore(")
        self.assertFalse(portability.cooperative_contract_errors(sources))
        sources["include/RV3032/RV3032.h"] = sources[
            "include/RV3032/RV3032.h"].replace("return nested();", "return readRegs(0, data, 1);")
        self.assertTrue(any("untimed register I/O in helper()" in error
                            for error in portability.cooperative_contract_errors(sources)))

    def test_member_qualifiers_and_trailing_return_cannot_hide_io(self):
        engines = '''
Status RV3032::pollJob() { return helper(); }
Status RV3032::processEeprom() { return Ok(); }
Status RV3032::processPersistent() { return Ok(); }
'''
        for signature in ("Status helper() &", "auto helper() -> Status",
                          "auto helper() & noexcept(true) -> decltype(Ok())"):
            for inline in (False, True):
                with self.subTest(signature=signature, inline=inline):
                    body = " { return readRegs(0, data, 1); }"
                    helper = ("class RV3032 final { " + signature + body + " };"
                              if inline else signature.replace("helper", "RV3032::helper") + body)
                    errors = portability.cooperative_contract_errors(engines + helper)
                    self.assertEqual(1, len(errors))
                    self.assertIn("untimed register I/O in helper()", errors[0])
                    self.assertFalse(portability.cooperative_contract_errors(
                        (engines + helper).replace("readRegs(", "readRegsBefore(")))

    def test_conditional_qualified_call_is_not_a_member_definition(self):
        source = '''
Status RV3032::pollJob() { return helper(); }
Status RV3032::processEeprom() { return Ok(); }
Status RV3032::processPersistent() { return Ok(); }
Status RV3032::helper() { return Ok(); }
Status RV3032::ordinaryRead() {
  if (RV3032::helper()) { return readRegs(0, data, 1); }
  return Ok();
}
auto RV3032::_updateHealth(const Status& st) -> Status { return st; }
class RV3032 final {
  auto _updateHealth(const Status& st) -> Status;
};
'''
        self.assertFalse(portability.cooperative_contract_errors(source))
        self.assertTrue(any("health update outside" in error for error in
                            portability.cooperative_contract_errors(source.replace(
                                "return helper();", "return RV3032::_updateHealth(st);"))))


class AbiChecks(unittest.TestCase):
    def test_commented_enum_cannot_supply_the_active_baseline(self):
        expected = "enum class DriverState : uint8_t { UNINIT, READY, DEGRADED, OFFLINE };"
        changed = expected.replace("READY,", "READY = 9,")
        for documented in ("/* Old layout: " + expected + " */\n",
                           "// Old layout: " + expected + "\n"):
            with self.subTest(documented=documented):
                self.assertEqual(9, abi.parse_enum_values(documented + changed, "DriverState")["READY"])
                with self.assertRaises(ValueError):
                    abi.parse_enum_values(documented, "DriverState")

    def test_every_operation_timeout_default_is_guarded(self):
        header = "\n".join(
            (abi.ROOT / "include/RV3032" / name).read_text(encoding="utf-8")
            for name in ("Status.h", "Config.h", "RV3032.h"))
        self.assertFalse(abi.public_contract_errors(header))
        for name, default in abi.EXPECTED_OPERATION_TIMEOUT_DEFAULTS.items():
            with self.subTest(method=name):
                pattern = (rf"(\b{name}\s*\([^;{{}}]*?\boperationTimeoutMs)"
                           rf"\s*=\s*{re.escape(default)}\b")
                for replacement in (r"\g<1> = 98765", r"\g<1>"):
                    changed, count = re.subn(pattern, replacement, header, count=1)
                    self.assertEqual(1, count)
                    self.assertTrue(any(
                        f"{name} operationTimeoutMs default changed" in error
                        for error in abi.public_contract_errors(changed)))

    def test_every_enum_and_constant_is_guarded(self):
        header = "\n".join(
            (abi.ROOT / "include/RV3032" / name).read_text(encoding="utf-8")
            for name in ("Status.h", "Config.h", "RV3032.h"))
        self.assertFalse(abi.public_contract_errors(header))
        for name in abi.EXPECTED_ENUM_VALUES:
            with self.subTest(enum=name):
                changed = header.replace(f"enum class {name} : uint8_t",
                                         f"enum class {name} : uint16_t")
                self.assertTrue(abi.public_contract_errors(changed))
        for name, (_, value) in abi.EXPECTED_PUBLIC_CONSTANTS.items():
            with self.subTest(constant=name):
                changed = header.replace(f"{name} = {value};", f"{name} = {value + 1};")
                self.assertTrue(abi.public_contract_errors(changed))
        self.assertTrue(abi.public_contract_errors(
            header.replace("Hz32768 = 0", "Hz32768 = 7")))
        self.assertTrue(abi.public_contract_errors(
            header.replace("  READY,", "  READY = 4,")))


class PackageChecks(unittest.TestCase):
    def make_fixture(self, root):
        for rel in set(package.REQUIRED_SOURCE_FILES + package.REQUIRED_PACKAGE_FILES):
            target = root / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text("required file fixture\n", encoding="utf-8")
        for rel in ("Doxyfile", "library.json", "idf_component.yml", "tools/hil_cli_runner.py",
                    "include/RV3032/RV3032.h", "include/RV3032/Version.h"):
            target = root / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((package.ROOT / rel).read_bytes())

    def test_completed_reports_cannot_return_to_source_or_package(self):
        artifacts = (
            "docs/CODE_AUDIT.md", "docs/CODE_AUDIT_RESOLUTION.md",
            "docs/CODE_AUDIT_FOLLOWUP.md", "docs/prompts/review.md",
            "docs/extracted-md/manual/chapter.md", "docs/reports/HIL_SUMMARY.md",
            "docs/reports/nested/session.csv",
        )
        with TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_fixture(root)
            with patch.object(package, "ROOT", root):
                with redirect_stdout(io.StringIO()):
                    self.assertEqual(0, package.source_check())
                for rel in artifacts:
                    with self.subTest(artifact=rel):
                        target = root / rel
                        target.parent.mkdir(parents=True, exist_ok=True)
                        target.write_text("completed run\n", encoding="utf-8")
                        output = io.StringIO()
                        with redirect_stdout(output):
                            self.assertEqual(1, package.source_check())
                        self.assertIn(f"completed workflow artifact remains: {rel}", output.getvalue())
                        self.assertTrue(package.forbidden_package_path(rel))
                        target.unlink()
        self.assertIn("docs/VERIFICATION.md", package.REQUIRED_SOURCE_FILES)
        self.assertIn("docs/VERIFICATION.md", package.REQUIRED_PACKAGE_FILES)
        self.assertFalse(package.forbidden_package_path("docs/VERIFICATION.md"))
        for rel in ("docs/reference-pdfs/RV-3032-C7_datasheet.pdf",
                    "docs/reference-pdfs/RV-3032-C7_App-Manual.pdf"):
            self.assertIn(rel, package.REQUIRED_SOURCE_FILES)
            self.assertTrue(package.forbidden_package_path(rel))

    def test_promptset_is_source_only_and_never_packaged(self):
        # Permanent: executed promptsets stay as historical requirements.
        with TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_fixture(root)
            prompt = root / "docs/prompts/01_step.md"
            prompt.parent.mkdir(parents=True)
            prompt.write_text("planned step\n", encoding="utf-8")
            with patch.object(package, "ROOT", root):
                output = io.StringIO()
                with redirect_stdout(output):
                    self.assertEqual(1, package.source_check())
                self.assertIn("completed workflow artifact remains: docs/prompts/01_step.md",
                              output.getvalue())
                (root / "docs/prompts/README.md").write_text("index\n", encoding="utf-8")
                with redirect_stdout(io.StringIO()):
                    self.assertEqual(0, package.source_check())
                for rel in ("docs/prompts/nested/report.md", "docs/prompts/run.log"):
                    with self.subTest(artifact=rel):
                        target = root / rel
                        target.parent.mkdir(parents=True, exist_ok=True)
                        target.write_text("completed run\n", encoding="utf-8")
                        output = io.StringIO()
                        with redirect_stdout(output):
                            self.assertEqual(1, package.source_check())
                        self.assertIn(f"completed workflow artifact remains: {rel}", output.getvalue())
                        target.unlink()
        for rel in ("docs/prompts/README.md", "docs/prompts/01_step.md"):
            self.assertTrue(package.forbidden_package_path(rel))

    def test_idf_manifest_in_source_and_archive(self):
        with TemporaryDirectory() as directory:
            root = Path(directory) / "source"
            self.make_fixture(root)
            # Use a different version to catch accidental comparisons against
            # the checkout instead of the source or archive under test.
            library = root / "library.json"
            metadata = json.loads(library.read_text(encoding="utf-8"))
            old_version = metadata["version"]
            metadata["version"] = "91.2.3"
            library.write_text(json.dumps(metadata), encoding="utf-8")
            header = root / "include/RV3032/Version.h"
            header.write_text(header.read_text(encoding="utf-8").replace(old_version, "91.2.3"),
                              encoding="utf-8")
            manifest = root / "idf_component.yml"
            valid = manifest.read_text(encoding="utf-8").replace(
                f'version: "{old_version}"', 'version: "91.2.3"')
            for name, contents, error in (
                ("valid", valid, ""),
                ("missing file", None, "idf_component.yml"),
                ("missing target", valid.replace('  - "esp32s2"\n', ''), "targets"),
                ("edited dependency", valid.replace('">=6.0.1"', '">=5.0"'), "dependencies"),
                ("unquoted re-dump", valid.replace('"', ''), "version"),
                ("version mismatch", valid.replace('version: "91.2.3"', 'version: "1.0.0"'), "version"),
                ("backup", valid, "git restore -- idf_component.yml"),
            ):
                with self.subTest(name=name):
                    if contents is None:
                        manifest.unlink()
                    else:
                        manifest.write_text(contents, encoding="utf-8")
                    if name == "backup":
                        (root / "idf_component.yml.orig").write_text(valid, encoding="utf-8")
                    archive = Path(directory) / "fixture.tar"
                    with tarfile.open(archive, "w") as tar:
                        for rel in (*package.REQUIRED_PACKAGE_FILES, "idf_component.yml.orig"):
                            if (root / rel).is_file():
                                tar.add(root / rel, arcname=rel)
                    with patch.object(package, "ROOT", root), redirect_stdout(io.StringIO()) as source:
                        self.assertEqual(int(bool(error)), package.source_check())
                    with redirect_stdout(io.StringIO()) as packed:
                        self.assertEqual(int(bool(error)), package.package_check(archive))
                    if error:
                        self.assertIn(error, source.getvalue())
                        self.assertIn(error, packed.getvalue())


if __name__ == "__main__":
    unittest.main()
