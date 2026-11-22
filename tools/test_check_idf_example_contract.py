"""Mutation tests for the native example contract; no SDK or board is needed."""
from pathlib import Path
import re
from shutil import copyfile
from tempfile import TemporaryDirectory
import unittest

import check_idf_example_contract as checks


class NativeExampleContract(unittest.TestCase):
    def setUp(self):
        scratch = TemporaryDirectory()
        self.addCleanup(scratch.cleanup)
        self.root = Path(scratch.name)
        for rel in (*checks.REQUIRED_FILES, Path("src/RV3032.cpp")):
            target = self.root / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            copyfile(checks.ROOT / rel, target)
        self.assertEqual([], checks.contract_errors(self.root))

    def reject(self, path, mutate, message):
        target = self.root / path
        original = target.read_text(encoding="utf-8")
        changed = mutate(original)
        self.assertNotEqual(original, changed, f"mutation missed {path}")
        target.write_text(changed, encoding="utf-8")
        try:
            errors = checks.contract_errors(self.root)
            self.assertTrue(any(message in e for e in errors), f"{message} missing from {errors}")
        finally:
            target.write_text(original, encoding="utf-8")

    def test_missing_required_files(self):
        for path in checks.REQUIRED_FILES:
            with self.subTest(path=path):
                target = self.root / path
                data = target.read_bytes()
                target.unlink()
                self.assertIn(f"missing required file: {path.as_posix()}", checks.contract_errors(self.root))
                target.write_bytes(data)

    def test_component_registration_and_dependencies(self):
        for path in (Path("CMakeLists.txt"), checks.WRAPPER):
            with self.subTest(path=path):
                self.reject(path, lambda s: s.replace("src/RV3032.cpp", "wrong/RV3032.cpp"), "wrapper SRCS")
        self.reject(checks.WRAPPER, lambda s: s.replace("/../../../../..", "/.."), "wrapper SRCS")
        cmake = checks.EXAMPLE / "main/CMakeLists.txt"
        for dependency in checks.DEPENDENCIES | set(checks.CONSOLE_COMPONENTS.values()):
            with self.subTest(dependency=dependency):
                self.reject(cmake, lambda s: re.sub(rf"\b{dependency}\b", "unrelated", s), dependency)
        self.reject(cmake, lambda s: s.replace("${console_component}", ""), "esp32s3 native main REQUIRES")
        self.reject(cmake, lambda s: s.replace('STREQUAL "esp32s3"', 'STREQUAL "esp32s2"'),
                    "esp32s2 native main REQUIRES")

    def test_no_component_override_or_main_manifest(self):
        self.reject("idf_component.yml", lambda s: s + "\n  override_path: ../other\n", "override_path")
        self.reject("idf_component.yml", lambda s: s + "\n  dev: {override_path: ../local}\n", "override_path")
        path = self.root / checks.EXAMPLE / "main/idf_component.yml"
        path.write_text("dependencies: {}\n", encoding="utf-8")
        self.assertIn("main/idf_component.yml is forbidden", checks.contract_errors(self.root))

    def test_every_source_builds_the_same_engine(self):
        for filename in ("main.cpp", "Rv3032IdfI2c.cpp", checks.ENGINE.name):
            with self.subTest(filename=filename):
                self.reject(checks.EXAMPLE / "main/CMakeLists.txt",
                            lambda s: s.replace(filename, "other.cpp"), f"compile {filename} once")
        self.reject("platformio.ini", lambda s: s.replace("+<examples/01_basic_bringup_cli/**>", ""),
                    "must compile the Arduino shell")
        for section in ("env:esp32s2dev", "env:esp32s3dev", "env:native"):
            def exclude_engine(text):
                start = text.index(f"[{section}]")
                end = text.find("\n[", start)
                if end == -1:
                    end = len(text)
                # Replace this environment's local filter, or override its inherited one.
                stanza = re.sub(r"(?m)^build_src_filter\s*=.*(?:\n[ \t]+[^\n]*)*", "", text[start:end])
                return text[:start] + stanza + "\nbuild_src_filter = -<*>\n" + text[end:]
            with self.subTest(section=section):
                self.reject("platformio.ini", exclude_engine, f"{section} must compile")

    def test_neutral_engine_and_native_sources(self):
        for path, fragment in ((checks.ENGINE, "Serial.read();"), (checks.ENGINE.with_suffix(".h"), "String input;"),
                               (checks.PARSER, "String token;"),
                               (checks.IDF_MAIN, "#include <Arduino.h>"), (checks.IDF_MAIN, '#include "Wire.h"'),
                               (checks.IDF_MAIN, "Wire.begin();"),
                               (checks.IDF_MAIN, "TwoWire* bus;"), (checks.IDF_MAIN, 'F("text");'),
                               (checks.IDF_MAIN, "millis();"),
                               (checks.EXAMPLE / "main/Rv3032IdfI2c.cpp", "delay(1);")):
            with self.subTest(fragment=fragment):
                self.reject(path, lambda s: s + "\n" + fragment + "\n", "framework-specific code")

    def test_shells_run_one_command_owner(self):
        for path in (checks.ARDUINO, checks.IDF_MAIN):
            with self.subTest(path=path):
                self.reject(path, lambda s: re.sub(r"\.\s*poll\s*\(", ".wrong(", s), "run the shared engine")
                self.reject(path, lambda s: s + '\nif (strcmp(line, "help") == 0) printHelp();\n',
                            "command dispatch in shell")

    def test_runner_commands_must_have_live_dispatch(self):
        self.reject("tools/hil_cli_runner.py", lambda s: s + '\nStep("M", "test", "new 12", ("OK",))\n',
                    "missing HIL command in engine dispatch: new")
        self.reject(checks.ENGINE,
                    lambda s: re.sub(r'equals\(\s*command\s*,\s*"version"\s*\)',
                                     'false /* equals(command, "version") */', s),
                    "missing HIL command in engine dispatch: version")
        self.reject(checks.ENGINE,
                    lambda s: re.sub(r'(equals\(\s*command\s*,\s*"help"\s*\))',
                                     r'\1 || equals(command, "not_a_real_command")', s),
                    "negative HIL command unexpectedly dispatched")

    def test_target_defaults_manifest_and_readme(self):
        for filename, settings in checks.SDKCONFIG.items():
            for setting in settings:
                with self.subTest(filename=filename, setting=setting):
                    self.reject(checks.EXAMPLE / filename, lambda s: s.replace(setting, "# " + setting), setting)
        self.reject("idf_component.yml", lambda s: s.replace(">=6.0.1", ">=5.0"), "quoted ESP-IDF")
        for token in checks.README_TOKENS:
            with self.subTest(token=token):
                # A README phrase may wrap across lines.
                pattern = r"\s+".join(re.escape(word) for word in token.split())
                self.reject(checks.EXAMPLE / "README.md",
                            lambda s: re.sub(pattern, "removed", s, flags=re.I), f"missing {token!r}")


if __name__ == "__main__":
    unittest.main()
