"""Mutation tests for the native example contract; no SDK or board is needed."""
from pathlib import Path
import re
from shutil import copyfile
from tempfile import TemporaryDirectory
import unittest

import check_idf_example_contract as checks


class NativeCiContract(unittest.TestCase):
    def setUp(self):
        self.workflow = (checks.ROOT / checks.WORKFLOW).read_text(encoding="utf-8")
        self.assertEqual([], checks.ci_errors(self.workflow))

    def reject(self, job, original, replacement):
        # Mutate only one job; identical tokens in the others must not mask it.
        match = re.search(rf"(?ms)^  {job}:.*?(?=^  [\w-]+:$|\Z)", self.workflow)
        self.assertIsNotNone(match)
        start, end = match.span()
        block = match[0]
        self.assertIn(original, block)
        mutated = self.workflow[:start] + block.replace(original, replacement, 1) + self.workflow[end:]
        self.assertTrue(any(job in error for error in checks.ci_errors(mutated)), f"{job}: {original}")

    def test_idf_matrix_jobs(self):
        for job, path in (("esp-idf-basic", "examples/esp_idf/basic"),
                          ("esp-idf-package", "pkg/RV3032/examples/esp_idf/basic")):
            self.reject(job, "        target: [esp32s3, esp32s2]",
                        "        target: [esp32s3, esp32s2]\n        exclude:\n          - target: esp32s2")
            for token in ("runs-on: ubuntu-latest", "fail-fast: false", "target: [esp32s3, esp32s2]",
                          "uses: espressif/esp-idf-ci-action@v1.2.0", "esp_idf_version: v6.0.1",
                          "target: ${{ matrix.target }}", "command: idf.py set-target ${{ matrix.target }} build",
                          f"path: {path}"):
                with self.subTest(job=job, token=token):
                    self.reject(job, token, "# " + token)
        for token in ("uses: actions/checkout@v7", "uses: actions/upload-artifact@v4", "if: always()",
                      "name: esp-idf-build-logs-${{ matrix.target }}", "examples/esp_idf/basic/build/log/**",
                      "if-no-files-found: ignore"):
            with self.subTest(token=token):
                self.reject("esp-idf-basic", token, "# " + token)

    def test_validated_artifact_handoff(self):
        for job, tokens in (
            ("validate-library", ("run: python tools/check_idf_example_contract.py", "pio pkg pack",
                                  "python tools/check_package.py package", "uses: actions/upload-artifact@v4",
                                  "name: rv3032-library-package", "path: dist/RV3032-C7.tar.gz",
                                  "if-no-files-found: error")),
            ("esp-idf-package", ("needs: validate-library", "uses: actions/download-artifact@v4",
                                 "name: rv3032-library-package", "path: ${{ github.workspace }}", "mkdir -p",
                                 "tar -xzf", "test -f pkg/RV3032/CMakeLists.txt",
                                 "test -f pkg/RV3032/examples/esp_idf/basic/CMakeLists.txt",
                                 "working-directory:", "char COMMIT[]", "char STATUS[]")),
        ):
            for token in tokens:
                with self.subTest(job=job, token=token):
                    self.reject(job, token, "removed")
        self.reject("esp-idf-package", "    steps:", "    steps:\n      - name: Checkout\n        uses: actions/checkout@v7")
        self.reject("esp-idf-package", "    steps:", "    steps:\n      - uses: actions/checkout@v7")
        self.reject("esp-idf-package", "        uses: espressif", "        if: false\n        uses: espressif")
        self.reject("esp-idf-package", "    needs:", "    continue-on-error: true\n    needs:")
        pack = "pio pkg pack -o dist/RV3032-C7.tar.gz ."
        validate = "python tools/check_package.py package dist/RV3032-C7.tar.gz"
        self.reject("validate-library", pack + "\n          " + validate, validate + "\n          " + pack)


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
        self.reject("platformio.ini", lambda s: s.replace("+<examples/common/**>",
                                                         "+<examples/common/**>\n  -<examples/common>"),
                    "must compile Rv3032Cli.cpp")
        self.reject("platformio.ini", lambda s: s.replace("+<examples/common/**>", "+<examples/*.cpp>"),
                    "must compile Rv3032Cli.cpp")
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
                    if setting.endswith("=y"):
                        key = setting.split("=")[0]
                        self.reject(checks.EXAMPLE / filename, lambda s: s + f"\n# {key} is not set\n", setting)
            if filename != "sdkconfig.defaults":
                for setting in checks.SDKCONFIG["sdkconfig.defaults"]:
                    with self.subTest(filename=filename, override=setting):
                        key = setting.split("=")[0]
                        self.reject(checks.EXAMPLE / filename, lambda s: s + f"\n{key}=100\n", setting)
        self.reject("idf_component.yml", lambda s: s.replace(">=6.0.1", ">=5.0"), "quoted ESP-IDF")
        for token in checks.README_TOKENS:
            with self.subTest(token=token):
                # A README phrase may wrap across lines.
                pattern = r"\s+".join(re.escape(word) for word in token.split())
                self.reject(checks.EXAMPLE / "README.md",
                            lambda s: re.sub(pattern, "removed", s, flags=re.I), f"missing {token!r}")


if __name__ == "__main__":
    unittest.main()
