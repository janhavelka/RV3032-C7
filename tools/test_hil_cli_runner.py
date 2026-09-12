#!/usr/bin/env python3
"""Device-free regressions for serial framing and retained HIL evidence."""

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import hil_cli_runner as runner
from test_hil_health_snapshot import HEALTHY_SNAPSHOT


class Clock:
    def __init__(self):
        self.now = 0.0

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds


class Port:
    def __init__(self, chunks):
        self.chunks = list(chunks)
        self.writes = []

    @property
    def in_waiting(self):
        if self.chunks and isinstance(self.chunks[0], Exception):
            raise self.chunks.pop(0)
        return len(self.chunks[0]) if self.chunks else 0

    def read(self, _size):
        return self.chunks.pop(0)

    def write(self, data):
        self.writes.append(data)
        return len(data)

    def flush(self):
        pass


class RunnerTest(unittest.TestCase):
    def test_complete_memory_payload_and_known_pattern(self):
        self.assertIsNone(runner.response_payload_reason("reg 0x0D", "reg[0x0D] = 0x01\n> "))
        ram = "User RAM offset 0 len 4:\n  0x00: A5 5A 00 FF\n> "
        self.assertIsNone(runner.response_payload_reason("ram 0 4", ram, bytes.fromhex("A5 5A 00 FF")))
        self.assertIsNotNone(runner.response_payload_reason("ram 0 4", ram, bytes(4)))
        eeprom = "\n".join(f"  0x{base:02X}: " + "A5 " * 8 for base in range(0xcb, 0xeb, 8))
        self.assertIsNone(runner.response_payload_reason("eeprom", eeprom + "\n> "))

    def test_stress_requires_complete_counts_and_health_despite_prompt(self):
        cases = (
            ("stress 3", "OK: 3, FAIL: 0 (100% success)\nTotal success: 7 -> 10 (expected 10) OK\nTotal failures: 0 -> 0 (expected 0) OK\nDriver state: READY -> READY\nConsecutive failures: 0\n> "),
            ("stress_mix 3", "Total: OK=3, FAIL=0 (100% success)\nSuccess delta: +3 (ops had 3 OK results)\nFailure delta: +0 (ops had 0 FAIL results)\nDriver state: READY\nConsecutive failures: 0\n> "),
            ("selftest", "Selftest result: pass=12 fail=0 skip=1\n> "),
        )
        for command, output in cases:
            self.assertIsNone(runner.response_payload_reason(command, output), command)
            lines = output.splitlines(keepends=True)
            for index in range(len(lines) - 1):
                incomplete = "".join(lines[:index] + lines[index + 1:])
                self.assertIsNotNone(runner.response_payload_reason(command, incomplete), command)
            self.assertIsNotNone(runner.response_payload_reason(command, output.replace("3, FAIL", "2, FAIL").replace("pass=12", "pass=0")))

    def test_complete_prompt_does_not_replace_missing_memory_payload(self):
        cases = (
            ("ram 0 16", "User RAM offset 0 len 16:\n  0x00: 00 01 02 03 04 05 06 07\n> ", ("User RAM",)),
            ("eeprom", "User EEPROM (0xCB..0xEA):\n  0xCB: 00 01 02 03 04 05 06 07\n  0xDB: 00 01 02 03 04 05 06 07\n> ", ("EEPROM",)),
            ("reg 0x0D", "reg[0x0D] =\n> ", ("reg[0x0D]",)),
        )
        for command, output, expected in cases:
            session = runner.HilSession("fake", 115200, 0, .01, False)
            session.serial = Port([output.encode()]); session.framing_ok = True
            result = session.run_step(runner.Step("x", "memory", command, expected))
            self.assertEqual(result.status, "FAIL", command)

    def test_short_write_stops_before_reading_a_queued_response(self):
        class ShortPort(Port):
            def write(self, data):super().write(data);return len(data)-1
        session = runner.HilSession("fake", 115200, 0, .01, False)
        session.serial = ShortPort([b"Probe OK\n> "]);session.framing_ok = True
        result = session.run_step(runner.Step("x", "probe", "probe", ("Probe OK",)))
        self.assertEqual(result.status,"FAIL")
        self.assertFalse(session.framing_ok)
        self.assertEqual(session.serial.chunks,[b"Probe OK\n> "])

    def test_normal_health_requires_ready_and_zero_error_counters(self):
        healthy = HEALTHY_SNAPSHOT
        self.assertIsNone(runner.healthy_driver_reason(healthy))
        for old, new in (("READY", "DEGRADED"), ("READY", "OFFLINE"),
                         ("Consecutive Failures: 0", "Consecutive Failures: 1"),
                         ("Total Failures: 0", "Total Failures: 7"),
                         ("queue_fail=0", "queue_fail=1"),
                         ("Total Failures: 0\n", "")):
            self.assertIsNotNone(runner.healthy_driver_reason(healthy.replace(old, new)))

    def test_expected_text_without_prompt_cannot_pass_or_be_unknown(self):
        for allow_unknown in (False, True):
            self.assertEqual(runner.classify_output("Probe OK\n", ("Probe OK",), allow_unknown)[0], "FAIL")

    def test_expected_error_is_exact_and_does_not_mask_other_failure(self):
        expected = ("I2C timeout",)
        self.assertEqual(runner.classify_output("[E] I2C timeout\n> ", (), expected_error_tokens=expected)[0], "UNKNOWN")
        for text in (
            "[E] I2C timeout\n[E] cleanup failed\n> ",
            "[E] I2C timeout; cleanup FAILED\n> ",
            "[E] I2C timeout\nSelftest result: fail=1\n> ",
            "[E] I2C timeout\n",
        ):
            self.assertEqual(runner.classify_output(text, (), expected_error_tokens=expected)[0], "FAIL")

    def test_prompt_can_arrive_in_later_chunk(self):
        clock = Clock()
        port = Port([b"Probe OK\n>", b" "])
        received = []
        with patch.object(runner, "time", clock):
            text = runner.read_command_response(port, .01, 1, ("Probe OK",), on_chunk=received.append)
        self.assertTrue(runner.has_prompt(text))
        self.assertEqual(b"".join(received), b"Probe OK\n> ")
        self.assertEqual(port.writes, [])

    def test_timeout_never_injects_sync_and_latches_session(self):
        clock = Clock()
        session = runner.HilSession("fake", 115200, 0, .01, False)
        session.serial = Port([b"Probe OK\n"])
        session.framing_ok = True
        with patch.object(runner, "time", clock):
            result = session.run_step(runner.Step("x", "probe", "probe", ("Probe OK",), timeout_s=.2))
            self.assertEqual(result.status, "FAIL")
            self.assertFalse(session.framing_ok)
            with self.assertRaisesRegex(RuntimeError, "refusing further"):
                session.run_step(runner.Step("y", "health", "drv", ("Driver Health",)))
        self.assertEqual(session.serial.writes, [b"probe\n"])
        self.assertLess(clock.now, .25)

    def test_partial_serial_failure_is_retained_live(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "live.log"
            session = runner.HilSession("fake", 115200, 0, .01, False)
            session.transcript_file = path.open("xb")
            session.serial = Port([b"Driver Health\nState: DEGRADED\n", OSError("disconnected")])
            session.framing_ok = True
            try:
                result = session.run_step(runner.Step("x", "health", "drv", ("Driver Health",)))
                retained = path.read_bytes()  # Read before closing or final artifacts.
                self.assertIn(b"State: DEGRADED", retained)
                self.assertIn(b"disconnected", retained)
                self.assertEqual(result.status, "FAIL")
                self.assertFalse(session.framing_ok)
                self.assertEqual(session.transcript, [])
            finally:
                session.transcript_file.close()

    def test_framed_failure_captures_health_and_stops_soak(self):
        clock = Clock()
        session = runner.HilSession("fake", 115200, 0, .01, False)
        session.serial = Port([b"[E] Probe FAILED\n> ", b"Driver Health\nState: OFFLINE\n> "])
        session.framing_ok = True
        with patch.object(runner, "time", clock):
            results, stats = runner.run_soak(session, (runner.Step("x", "probe", "probe", ("Probe OK",)),), 30, 0)
        self.assertEqual(session.serial.writes, [b"probe\n", b"drv\n"])
        self.assertEqual([result.status for result in results], ["FAIL", "PASS"])
        self.assertEqual(stats.fail_count, 1)
        self.assertIn("State: OFFLINE", "".join(session.transcript))

    def test_reboot_banner_latches_even_when_prompt_returns(self):
        session = runner.HilSession("fake", 115200, 0, .01, False)
        session.serial = Port([b"ESP-ROM:esp32s3\nProbe OK\n> "])
        session.framing_ok = True
        result = session.run_step(runner.Step("x", "probe", "probe", ("Probe OK",)))
        self.assertEqual(result.status, "FAIL")
        self.assertFalse(session.framing_ok)

    def test_unframed_failure_sends_no_health_command(self):
        clock = Clock()
        session = runner.HilSession("fake", 115200, 0, .01, False)
        session.serial = Port([b"[E] Probe FAILED\n"])
        session.framing_ok = True
        with patch.object(runner, "time", clock):
            results, _stats = runner.run_soak(session, (runner.Step("x", "probe", "probe", ("Probe OK",), timeout_s=.1),), 30, 0)
        self.assertEqual(len(results), 1)
        self.assertEqual(session.serial.writes, [b"probe\n"])


if __name__ == "__main__":
    unittest.main()
