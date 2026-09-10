"""Mutation probes for the maintained source/ABI contract checks."""
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


class AbiChecks(unittest.TestCase):
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
