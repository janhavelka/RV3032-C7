#!/usr/bin/env python3
"""Check the native example and its shared CLI against the integration contract."""
from __future__ import annotations

import ast
import configparser
import glob
from pathlib import Path
import re
import sys

from check_portability import CODE_SUFFIXES, mask_non_code, matching_brace

ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = Path(".github/workflows/ci.yml")
EXAMPLE = Path("examples/esp_idf/basic")
WRAPPER = EXAMPLE / "components/RV3032/CMakeLists.txt"
ARDUINO = Path("examples/01_basic_bringup_cli/main.cpp")
ENGINE = Path("examples/common/Rv3032Cli.cpp")
PARSER = ENGINE.with_name("CommandHandler.h")
IDF_MAIN = EXAMPLE / "main/main.cpp"
SDKCONFIG = {
    "sdkconfig.defaults": ("CONFIG_FREERTOS_HZ=1000", "CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192"),
    "sdkconfig.defaults.esp32s2": ("CONFIG_ESP_CONSOLE_USB_CDC=y", "CONFIG_ESP_CONSOLE_SECONDARY_NONE=y"),
    "sdkconfig.defaults.esp32s3": ("CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y", "CONFIG_ESP_CONSOLE_SECONDARY_NONE=y"),
}
REQUIRED_FILES = (
    Path("CMakeLists.txt"), Path("idf_component.yml"), Path("platformio.ini"), WORKFLOW,
    ARDUINO, ENGINE, ENGINE.with_suffix(".h"), PARSER, Path("tools/hil_cli_runner.py"),
    WRAPPER, IDF_MAIN, EXAMPLE / "CMakeLists.txt", EXAMPLE / "README.md",
    EXAMPLE / "main/CMakeLists.txt", EXAMPLE / "main/GenerateBuildIdentity.cmake",
    EXAMPLE / "main/Rv3032IdfI2c.cpp", EXAMPLE / "main/Rv3032IdfI2c.h",
    *(EXAMPLE / name for name in SDKCONFIG),
)
DEPENDENCIES = {"RV3032", "esp_driver_i2c", "esp_driver_gpio", "esp_timer", "freertos", "spi_flash", "esp_psram"}
CONSOLE_COMPONENTS = {"esp32s3": "esp_driver_usb_serial_jtag", "esp32s2": "esp_usb_cdc_rom_console"}
README_TOKENS = (
    r"C:\esp\idf.cmd 6.0.1", "set-target", "build", "flash", "monitor", "ESP32-S2",
    "ESP32-S3", "USB CDC", "USB Serial/JTAG", "GPIO8", "GPIO9", "400 kHz", "0x51",
    "50 ms", "external pull-ups", "help", "version", "probe", "EEPROM", "disabled",
    "primary-cell ensure CONFIRM-PRIMARY-CELL", "no Arduino sources", "hardware validation",
    "stack high-water mark", "NOT RUN",
)


def cmake_register(text: str, key: str) -> list[str]:
    """Read a keyword list from this example's idf_component_register call."""
    text = re.sub(r"(?m)#.*$", "", text)
    call = re.search(r"\bidf_component_register\s*\(([^)]*)\)", text)
    if not call:
        return []
    values = []
    active = False
    for word in call[1].split():
        if word in ("SRCS", "INCLUDE_DIRS", "PRIV_INCLUDE_DIRS", "REQUIRES", "PRIV_REQUIRES"):
            active = word == key
        elif active:
            values.append(word.strip('"'))
    return values


def native_requires(cmake: str, target: str) -> set[str]:
    """Resolve the example's target-selected USB component in REQUIRES."""
    code = re.sub(r"(?m)#.*$", "", cmake)
    target_var = re.search(r"\bidf_build_get_property\(\s*(\w+)\s+IDF_TARGET\s*\)", code)
    if target_var:
        branch = re.search(
            rf'\bif\(\s*{target_var[1]}\s+STREQUAL\s+"?(\w+)"?\s*\)(.*?)'
            r'\belse\(\)(.*?)\bendif\(\)', code, re.DOTALL)
        if branch:
            body = branch[2] if target == branch[1] else branch[3]
            for var, component in re.findall(r'\bset\(\s*(\w+)\s+"?(\w+)"?\s*\)', body):
                code = code.replace("${" + var + "}", component)
    return set(cmake_register(code, "REQUIRES"))


def hil_commands(text: str) -> tuple[set[str], set[str]]:
    """Collect the commands from every Step, including the negative control."""
    known, unknown = set(), set()
    for call in ast.walk(ast.parse(text)):
        if not isinstance(call, ast.Call) or not isinstance(call.func, ast.Name) or call.func.id != "Step":
            continue
        command = ast.literal_eval(call.args[2])
        if command:
            expected = ast.literal_eval(call.args[3])
            (unknown if "Unknown command" in expected else known).add(command.split()[0])
    return known, unknown


def compiled_by_pio(root: Path, config: configparser.ConfigParser, env: str, path: Path) -> bool:
    filters = config.get(env, "build_src_filter", fallback=config["env"]["build_src_filter"])
    selected = False
    for operation, pattern in re.findall(r"([+-])<([^>]+)>", filters):
        # PlatformIO walks every directory selected by a glob. Matching a
        # parent selects this source even when the pattern ends at the parent.
        if any(Path(match) == path or Path(match) in path.parents
               for match in glob.iglob(pattern, root_dir=root, recursive=True)):
            selected = operation == "+"
    return env in config and selected


def ci_errors(workflow: str) -> list[str]:
    """Check job-scoped tokens and artifact handoff in execution order."""
    matrix = ("runs-on: ubuntu-latest", "fail-fast: false", "target: [esp32s3, esp32s2]")
    artifact = "name: rv3032-library-package"
    upload = "uses: actions/upload-artifact@v4"
    native_build = ("uses: espressif/esp-idf-ci-action@v1.2.0", "esp_idf_version: v6.0.1",
                    "target: ${{ matrix.target }}", "command: idf.py set-target ${{ matrix.target }} build")
    pack = "pio pkg pack -o dist/RV3032-C7.tar.gz ."
    validate = "python tools/check_package.py package dist/RV3032-C7.tar.gz"
    extract = 'mkdir -p "$GITHUB_WORKSPACE/pkg/RV3032"'
    unpack = 'tar -xzf RV3032-C7.tar.gz -C "$GITHUB_WORKSPACE/pkg/RV3032"'
    required = {
        "validate-library": ((), [
            ("run: python tools/check_idf_example_contract.py",),
            (pack, validate),
            (upload, artifact, "path: dist/RV3032-C7.tar.gz", "if-no-files-found: error"),
        ]),
        "esp-idf-basic": (matrix, [
            ("uses: actions/checkout@v7",),
            (*native_build, "path: examples/esp_idf/basic"),
            (upload, "if: always()", "name: esp-idf-build-logs-${{ matrix.target }}",
             "examples/esp_idf/basic/build/log/**", "if-no-files-found: ignore"),
        ]),
        "esp-idf-package": ((*matrix, "needs: validate-library"), [
            ("uses: actions/download-artifact@v4", artifact, "path: ${{ github.workspace }}"),
            (extract, unpack, "test -f pkg/RV3032/CMakeLists.txt",
             "test -f pkg/RV3032/examples/esp_idf/basic/CMakeLists.txt"),
            (*native_build, "path: pkg/RV3032/examples/esp_idf/basic"),
            ("working-directory: pkg/RV3032/examples/esp_idf/basic/build/esp-idf/main",
             """grep -Fx 'inline constexpr char COMMIT[] = "unknown";' Rv3032BuildIdentity.h""",
             """grep -Fx 'inline constexpr char STATUS[] = "unknown";' Rv3032BuildIdentity.h"""),
        ]),
    }
    errors = []
    # These jobs use the workflow's existing two-space mapping and named steps.
    parts = re.split(r"(?m)^  ([\w-]+):[ \t]*$", workflow.partition("\njobs:\n")[2])
    jobs = dict(zip(parts[1::2], parts[2::2]))
    for name, (settings, required_steps) in required.items():
        if name not in jobs:
            errors.append(f"CI missing job {name}")
            continue
        header, *blocks = re.split(r"(?m)^      - name:", jobs[name])
        header_lines = {line.strip() for line in header.splitlines()}
        for setting in settings:
            if setting not in header_lines:
                errors.append(f"CI {name} missing {setting}")
        excluded = re.search(r"(?m)^        exclude:([^\n]*(?:\n        [ -][^\n]*)*)", header)
        if settings and excluded and re.search(r"""\btarget:\s*["']?esp32s[23]\b""", excluded[1]):
            errors.append(f"CI {name} excludes a required target")
        if name == "esp-idf-package" and re.search(
                r"""(?im)^\s*(?:-\s*)?uses:\s*["']?actions/checkout@""", jobs[name]):
            errors.append("CI esp-idf-package must build the archive without checkout")
        steps = iter([line.strip() for line in block.splitlines()] for block in blocks)
        for first, *tokens in required_steps:
            step = next((lines for lines in steps if first in lines), None)
            if step is None:
                errors.append(f"CI {name} missing or out-of-order step: {first}")
                break
            for token in tokens:
                if token not in step:
                    errors.append(f"CI {name} step {first} missing {token}")
            if first in (pack, extract) and any(step.index(token) < step.index(first)
                                                for token in tokens if token in step):
                errors.append(f"CI {name} commands out of order after {first}")
            for line in header_lines.union(step):
                if ((line.startswith("if:") and line not in tokens) or
                        line.startswith("continue-on-error: true")):
                    errors.append(f"CI {name} required step {first} may be skipped or ignored")
    return errors


def contract_errors(root: Path) -> list[str]:
    errors = [f"missing required file: {path.as_posix()}" for path in REQUIRED_FILES
              if not (root / path).is_file()]
    if errors:
        return errors

    def read(path: Path) -> str:
        return (root / path).read_text(encoding="utf-8")

    errors.extend(ci_errors(read(WORKFLOW)))
    root_cmake = read(Path("CMakeLists.txt"))
    wrapper = read(WRAPPER)
    # Resolve the wrapper's actual paths. Matching just filenames could accept
    # a different, local copy of the driver.
    variable = re.search(r'(?m)^\s*set\(rv3032_root\s+"([^"]+)"\)', wrapper)
    wrapper_dir = (root / WRAPPER).parent
    if variable:
        wrapper = wrapper.replace("${rv3032_root}", variable[1])
    sources = [(wrapper_dir / name.replace("${CMAKE_CURRENT_LIST_DIR}/", "")).resolve()
               for name in cmake_register(wrapper, "SRCS")]
    root_sources = [(root / name).resolve() for name in cmake_register(root_cmake, "SRCS")]
    if not sources or sorted(sources) != sorted(root_sources) or not all(p.is_file() for p in sources):
        errors.append("fixed-name RV3032 wrapper SRCS differ from the root component")
    main = read(EXAMPLE / "main/CMakeLists.txt")
    for target, console in CONSOLE_COMPONENTS.items():
        missing = (DEPENDENCIES | {console}) - native_requires(main, target)
        if missing:
            errors.append(f"{target} native main REQUIRES missing {sorted(missing)}")
    main_dir = (root / IDF_MAIN).parent
    native_sources = [(main_dir / name).resolve() for name in cmake_register(main, "SRCS")]
    for source in (IDF_MAIN, ENGINE, EXAMPLE / "main/Rv3032IdfI2c.cpp"):
        if native_sources.count((root / source).resolve()) != 1:
            errors.append(f"native main must compile {source.name} once")
    cfg = configparser.ConfigParser(interpolation=None)
    cfg.read(root / "platformio.ini", encoding="utf-8")
    for env in ("env:esp32s2dev", "env:esp32s3dev", "env:native"):
        if not compiled_by_pio(root, cfg, env, ENGINE):
            errors.append(f"{env} must compile {ENGINE.name}")
        if env != "env:native" and not compiled_by_pio(root, cfg, env, ARDUINO):
            errors.append(f"{env} must compile the Arduino shell")

    manifest = read(Path("idf_component.yml"))
    if not re.search(r'(?m)^dependencies:\n  idf: ">=6\.0\.1"$', manifest):
        errors.append("manifest must declare the quoted ESP-IDF >=6.0.1 dependency")
    if (root / EXAMPLE / "main/idf_component.yml").exists():
        errors.append("main/idf_component.yml is forbidden")
    example = root / EXAMPLE
    # Inspect maintained sources only; idf.py can leave generated SDK and
    # managed dependency copies alongside the project.
    example_files = []
    for child in example.iterdir():
        if child.is_file():
            example_files.append(child)
        elif child.name != "managed_components" and not child.name.startswith("build"):
            example_files.extend(p for p in child.rglob("*") if p.is_file())
    for path in [root / "idf_component.yml", *(p for p in example_files if p.name == "idf_component.yml")]:
        yaml = re.sub(r"(?m)#.*$", "", path.read_text(encoding="utf-8"))
        if re.search(r"""\boverride_path["']?\s*:""", yaml):
            errors.append(f"override_path is forbidden in {path.relative_to(root)}")

    for filename, settings in SDKCONFIG.items():
        lines = read(EXAMPLE / filename).splitlines()
        # IDF loads the target defaults after the common defaults.
        for setting in set(settings) | set(SDKCONFIG["sdkconfig.defaults"]):
            key = setting.split("=")[0]
            declarations = [line for line in lines if line.startswith(key + "=")
                            or line == f"# {key} is not set"]
            if (setting in settings or declarations) and declarations != [setting]:
                errors.append(f"{filename} must set {setting}")
    readme = " ".join(read(EXAMPLE / "README.md").lower().split())
    for token in README_TOKENS:
        if token.lower() not in readme:
            errors.append(f"example README missing {token!r}")

    neutral = [root / ENGINE, (root / ENGINE).with_suffix(".h"), root / PARSER]
    neutral.extend(p for p in example_files if p.suffix.lower() in CODE_SUFFIXES)
    for path in neutral:
        raw = path.read_text(encoding="utf-8")
        code = mask_non_code(raw)
        includes = re.finditer(r'(?m)^\s*(#)\s*include\s*[<"](?:Arduino|Wire)\.h[>"]', raw)
        if any(code[m.start(1)] == "#" for m in includes) or re.search(
                r"\b(Wire|TwoWire|String|Serial|millis)\b|\b(delay|F)\s*\(", code):
            errors.append(f"framework-specific code in {path.relative_to(root)}")

    engine = read(ENGINE)
    code = mask_non_code(engine)
    dispatch = re.search(r"\bEngine::dispatchLine\s*\([^)]*\)\s*\{", code)
    if not dispatch:
        errors.append("missing shared Engine::dispatchLine")
    else:
        end = matching_brace(code, dispatch.end() - 1)
        # Only active tests in the dispatch body count. Help rows and comments
        # can still contain a command's spelling after its handler is removed.
        body = engine[dispatch.end():end]
        masked_body = code[dispatch.end():end]
        names = set()
        for branch in re.finditer(r"\bif\s*\(([^;{}]+?)\)\s*(?:\{|return\b)", masked_body):
            condition = body[branch.start(1):branch.end(1)]
            for test in re.finditer(r'\bequals\s*\(\s*command\s*,\s*"([^"]+)"\s*\)', condition):
                if masked_body.startswith("equals", branch.start(1) + test.start()):
                    names.add(test[1])
        known, unknown = hil_commands(read(Path("tools/hil_cli_runner.py")))
        for command in sorted(known - names):
            errors.append(f"missing HIL command in engine dispatch: {command}")
        if names & unknown:
            errors.append(f"negative HIL command unexpectedly dispatched: {sorted(names & unknown)}")

    for path in (ARDUINO, IDF_MAIN):
        shell = read(path)
        code = mask_non_code(shell)
        if not re.search(r"\bEngine\b", code) or not re.search(r"\.\s*poll\s*\(", code):
            errors.append(f"{path.as_posix()} must run the shared engine")
        # A shell may forward bytes to the engine, but cannot select handlers.
        selects_handler = re.search(r"\b(?:cmd_\w+|cmd[A-Z]\w*|process_command|dispatch(?:Line)?)\s*\(", code)
        compares_command = re.finditer(
            r'\b(?:strcmp|equals)\s*\(\s*(?:cmd|command|token|line|verb)\s*,\s*"'
            r'|\b(?:cmd|command|token|line|verb)\s*==\s*"', shell)
        if selects_handler or any(not code[m.start()].isspace() for m in compares_command):
            errors.append(f"command dispatch in shell {path.as_posix()}")
    return errors


def main() -> int:
    errors = contract_errors(ROOT)
    for error in errors:
        print(f"- {error}")
    print("IDF example contract FAILED" if errors else "IDF example contract PASSED")
    return int(bool(errors))


if __name__ == "__main__":
    sys.exit(main())
