"""Mutation probes for the maintained source/ABI contract checks."""
import re
import unittest

import check_abi as abi
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


if __name__ == "__main__":
    unittest.main()
