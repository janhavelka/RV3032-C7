#!/usr/bin/env python3
"""Device-free regressions for the actual Arduino cmd_drv health snapshot."""

import unittest

import hil_cli_runner as runner


# Field names, order and boolean spellings come from
# examples/01_basic_bringup_cli/main.cpp::cmd_drv. Values are synthetic.
HEALTHY_SNAPSHOT = """
=== Driver Health ===
State: READY
isInitialized: yes
Config: addr=0x51 i2cTimeout=50 offlineThreshold=5 nowMs=yes
EEPROM: busy=no generic_persistence=no timeout=100 queue=0 queue_ok=0 queue_fail=0

=== Counters ===
Consecutive Failures: 0
Total Successes: 137
Total Failures: 0
Success rate: 100.0%

=== Timestamps ===
Last OK: 12 ms ago (at 1000 ms)
Last Error: never

=== Last Error Details ===
Code: OK (0)
Detail: 0
Message: OK

=== EEPROM State ===
Busy: false
Status: OK

> """


class HealthSnapshotTest(unittest.TestCase):
    def test_actual_complete_snapshot(self):
        self.assertIsNone(runner.healthy_driver_reason(HEALTHY_SNAPSHOT))

    def test_actual_ansi_crlf_snapshot(self):
        colored = HEALTHY_SNAPSHOT.replace("READY", "\x1b[32mREADY\x1b[0m")
        colored = colored.replace("137", "\x1b[32m137\x1b[0m")
        self.assertIsNone(runner.healthy_driver_reason(colored.replace("\n", "\r\n")))

    def test_fresh_passive_begin_has_no_counted_success_or_timestamp(self):
        fresh = HEALTHY_SNAPSHOT.replace("137", "0").replace("100.0%", "0.0%")
        fresh = fresh.replace("12 ms ago (at 1000 ms)", "never")
        self.assertIsNone(runner.healthy_driver_reason(fresh))

    def test_every_required_line_missing_despite_final_prompt(self):
        for line in HEALTHY_SNAPSHOT.splitlines(keepends=True):
            if not line.strip() or line.startswith(">"):
                continue
            with self.subTest(line=line.strip()):
                self.assertIsNotNone(runner.healthy_driver_reason(HEALTHY_SNAPSHOT.replace(line, "", 1)))

    def test_every_required_line_duplicated_despite_final_prompt(self):
        for line in HEALTHY_SNAPSHOT.splitlines(keepends=True):
            if not line.strip() or line.startswith(">"):
                continue
            with self.subTest(line=line.strip()):
                self.assertIsNotNone(runner.healthy_driver_reason(HEALTHY_SNAPSHOT.replace(line, line * 2, 1)))

    def test_every_required_line_malformed_despite_final_prompt(self):
        for line in HEALTHY_SNAPSHOT.splitlines(keepends=True):
            if not line.strip() or line.startswith(">"):
                continue
            with self.subTest(line=line.strip()):
                bad = line.rstrip("\n") + " truncated\n"
                self.assertIsNotNone(runner.healthy_driver_reason(HEALTHY_SNAPSHOT.replace(line, bad, 1)))

    def test_missing_row_replaced_by_malformed_duplicate(self):
        bad = HEALTHY_SNAPSHOT.replace("Detail: 0\n", "Total Successes: ???\n")
        self.assertIsNotNone(runner.healthy_driver_reason(bad))

    def test_unhealthy_or_inconsistent_fields(self):
        cases = (
            ("State: READY", "State: DEGRADED"),
            ("State: READY", "State: OFFLINE"),
            ("isInitialized: yes", "isInitialized: no"),
            ("Consecutive Failures: 0", "Consecutive Failures: 1"),
            ("Total Failures: 0", "Total Failures: 1"),
            ("Success rate: 100.0%", "Success rate: 0.0%"),
            ("Total Successes: 137", "Total Successes: 0"),
            ("Last Error: never", "Last Error: 5 ms ago (at 1007 ms)"),
            ("Code: OK (0)", "Code: I2C_BUS (7)"),
            ("Code: OK (0)", "Code: OK (7)"),
            ("Detail: 0", "Detail: 4"),
            ("Message: OK", "Message: (none)"),
            ("busy=no", "busy=yes"),
            ("queue=0", "queue=1"),
            ("queue_fail=0", "queue_fail=1"),
            ("Busy: false", "Busy: true"),
            ("Status: OK", "Status: EEPROM timeout"),
        )
        for old, new in cases:
            with self.subTest(new=new):
                self.assertIsNotNone(runner.healthy_driver_reason(HEALTHY_SNAPSHOT.replace(old, new)))

    def test_malformed_and_out_of_range_numeric_fields(self):
        cases = (
            ("Total Successes: 137", "Total Successes: -1"),
            ("Total Successes: 137", "Total Successes: 4294967296"),
            ("Total Successes: 137", "Total Successes: 1.0"),
            ("Total Successes: 137", "Total Successes: nan"),
            ("i2cTimeout=50", "i2cTimeout=0"),
            ("i2cTimeout=50", "i2cTimeout=101"),
            ("i2cTimeout=50", "i2cTimeout=4294967296"),
            ("offlineThreshold=5", "offlineThreshold=0"),
            ("offlineThreshold=5", "offlineThreshold=256"),
            ("timeout=100", "timeout=0"),
            ("timeout=100", "timeout=9"),
            ("timeout=100", "timeout=251"),
            ("timeout=100", "timeout=4294967296"),
            ("queue_ok=0", "queue_ok=4294967296"),
            ("12 ms ago", "4294967296 ms ago"),
            ("at 1000 ms", "at 4294967296 ms"),
            ("at 1000 ms", "at 0 ms"),
            ("addr=0x51", "addr=0xGG"),
            ("addr=0x51", "addr=0x50"),
            ("nowMs=yes", "nowMs=maybe"),
        )
        for old, new in cases:
            with self.subTest(new=new):
                self.assertIsNotNone(runner.healthy_driver_reason(HEALTHY_SNAPSHOT.replace(old, new)))

    def test_uint32_boundaries_and_wrap_safe_age(self):
        full = HEALTHY_SNAPSHOT.replace("137", "4294967295")
        full = full.replace("12 ms ago (at 1000 ms)", "4294967295 ms ago (at 4294967295 ms)")
        full = full.replace("queue_ok=0", "queue_ok=4294967295")
        self.assertIsNone(runner.healthy_driver_reason(full))

    def test_supported_configuration_values_are_not_campaign_hardcoded(self):
        full = HEALTHY_SNAPSHOT.replace("i2cTimeout=50", "i2cTimeout=100")
        full = full.replace("offlineThreshold=5", "offlineThreshold=255")
        full = full.replace("nowMs=yes", "nowMs=no")
        full = full.replace("generic_persistence=no", "generic_persistence=yes")
        full = full.replace("timeout=100", "timeout=200")
        self.assertIsNone(runner.healthy_driver_reason(full))

    def test_run_step_rejects_lost_middle_fields_with_complete_prompt(self):
        class Port:
            def __init__(self, data):
                self.data = data

            @property
            def in_waiting(self):
                return len(self.data)

            def read(self, size):
                data, self.data = self.data[:size], self.data[size:]
                return data

            def write(self, data):
                return len(data)

        for removed in ("Total Successes: 137\n", "Detail: 0\n", "Last Error: never\n"):
            session = runner.HilSession("fake", 115200, 0, 0.01, False)
            session.serial = Port(HEALTHY_SNAPSHOT.replace(removed, "").encode())
            session.framing_ok = True
            result = session.run_step(runner.Step("test", "health", "drv", ("Driver Health", "State:", "EEPROM:"), require_ready_health=True))
            with self.subTest(removed=removed.strip()):
                self.assertEqual(result.status, "FAIL")
                self.assertTrue(session.framing_ok)


if __name__ == "__main__":
    unittest.main()
