#!/usr/bin/env python3
"""Bounded serial HIL runner for the RV3032-C7 bring-up CLI.

This is intentionally small: it drives the existing example CLI, captures a
transcript, and emits a Markdown step table for the human HIL report.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import re
import statistics
import sys
import time
from dataclasses import dataclass, field, replace
from pathlib import Path
from typing import Callable, Iterable

ANSI_RE = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
PROMPT = "> "
FAILURE_PATTERNS = (
    "[E]",
    "FAILED",
    "[FAIL]",
    "MISMATCH",
    "RTC init failed",
    "Check I2C wiring",
    "Guru Meditation Error",
    "Brownout detector",
)
NONZERO_FAILURE_RE = re.compile(r"\bFAIL\s*[:=]\s*[1-9]\d*", re.IGNORECASE)


@dataclass(frozen=True)
class Step:
    test_id: str
    area: str
    command: str
    expected: tuple[str, ...]
    timeout_s: float = 5.0
    idle_timeout_s: float | None = None
    allow_unknown: bool = False
    expected_error_tokens: tuple[str, ...] = ()
    notes: str = ""
    require_ready_health: bool = False
    expected_data: bytes | None = None


@dataclass
class Result:
    test_id: str
    area: str
    command: str
    expected: str
    observed: str
    elapsed_s: float
    status: str
    notes: str = ""


@dataclass
class SoakStats:
    start: str
    end: str = ""
    duration_s: float = 0.0
    command_counts: dict[str, int] = field(default_factory=dict)
    pass_count: int = 0
    fail_count: int = 0
    unknown_count: int = 0
    not_run_count: int = 0
    latencies_s: list[float] = field(default_factory=list)
    worst_command: str = ""
    worst_latency_s: float = 0.0
    consecutive_failures_max: int = 0


SAFE_FUNCTIONAL_STEPS: tuple[Step, ...] = (
    Step("HIL-001", "boot", "", ("RTC address communication succeeded", "Type 'help'"), timeout_s=8.0),
    Step("HIL-002", "identity", "version", ("RV3032 library version",)),
    Step("HIL-003", "cli", "help", ("RV3032-C7 CLI Help", "selftest")),
    Step("HIL-004", "bus", "scan", ("Scan complete",), timeout_s=15.0),
    Step("HIL-005", "probe", "probe", ("Probe OK", "Health tracking: unchanged")),
    Step("HIL-006", "health", "drv", ("Driver Health", "State:", "EEPROM:"), require_ready_health=True),
    Step("HIL-007", "time", "time", ("Current time",), allow_unknown=True),
    Step("HIL-008", "time", "unix", ("Unix timestamp",), allow_unknown=True),
    Step("HIL-009", "temperature", "temp", ("Temperature",), allow_unknown=True),
    Step("HIL-010", "status", "status", ("Status register",)),
    Step("HIL-011", "status", "statusf", ("Status flags",)),
    Step("HIL-012", "validity", "validity", ("PORF:", "VLF:", "BSF:")),
    Step("HIL-013", "alarm", "alarm", ("Alarm",), allow_unknown=True),
    Step("HIL-014", "timer", "timer", ("Timer",), allow_unknown=True),
    Step("HIL-015", "clock", "clkout", ("Clock output",), allow_unknown=True),
    Step("HIL-016", "clock", "clkout_freq", ("Clock output frequency",), allow_unknown=True),
    Step("HIL-017", "calibration", "offset", ("Frequency offset",), allow_unknown=True),
    Step("HIL-018", "event", "evi", ("EVI",), allow_unknown=True),
    Step(
        "HIL-019",
        "timestamp",
        "ts tlow",
        ("timestamp",),
        allow_unknown=True,
        notes="No safe temperature-low stimulus fixture; empty/unset is valid terminal evidence.",
    ),
    Step(
        "HIL-020",
        "timestamp",
        "ts thigh",
        ("timestamp",),
        allow_unknown=True,
        notes="No safe temperature-high stimulus fixture; empty/unset is valid terminal evidence.",
    ),
    Step(
        "HIL-021",
        "timestamp",
        "ts evi",
        ("timestamp",),
        allow_unknown=True,
        notes="No external-event stimulus fixture; empty/unset is valid terminal evidence.",
    ),
    Step("HIL-022", "ram", "ram 0 16", ("User RAM",), allow_unknown=True),
    Step("HIL-023", "register", "reg 0x0D", ("reg[0x0D]",), allow_unknown=True),
    Step("HIL-024", "register", "reg 0x0E", ("reg[0x0E]",), allow_unknown=True),
    Step("HIL-025", "diagnostics", "selftest", ("Selftest result:", "fail=0"), timeout_s=15.0),
    Step("HIL-026", "validation", "not_a_real_command", ("Unknown command",), timeout_s=5.0),
)

STRESS_STEPS: tuple[Step, ...] = (
    Step("HIL-101", "stress", "stress 50", ("OK", "FAIL: 0"), timeout_s=30.0),
    Step("HIL-102", "stress", "stress_mix 50", ("Mixed Operations Stress Test", "FAIL=0"), timeout_s=30.0),
)

SOAK_COMMANDS: tuple[Step, ...] = (
    Step("SOAK-time", "soak", "time", ("Current time",), timeout_s=5.0, allow_unknown=True),
    Step("SOAK-temp", "soak", "temp", ("Temperature",), timeout_s=5.0, allow_unknown=True),
    Step("SOAK-status", "soak", "status", ("Status register",), timeout_s=5.0),
    Step("SOAK-validity", "soak", "validity", ("PORF:", "VLF:", "BSF:"), timeout_s=5.0),
    Step("SOAK-probe", "soak", "probe", ("Probe OK",), timeout_s=5.0),
    Step("SOAK-drv", "soak", "drv", ("Driver Health", "State:"), timeout_s=5.0, require_ready_health=True),
    Step("SOAK-stress", "soak", "stress 10", ("OK", "FAIL: 0"), timeout_s=15.0),
    Step("SOAK-mix", "soak", "stress_mix 10", ("Mixed Operations Stress Test", "FAIL=0"), timeout_s=15.0),
)

INTENSIVE_SETUP_STEPS: tuple[Step, ...] = (
    Step("HIL-D001", "destructive-setup", "setbuild", ("Build timestamp calendar set completed",), timeout_s=5.0),
    Step("HIL-D002", "destructive-setup", "clear_porf", ("power-on reset flag clear terminal status: OK",), timeout_s=5.0),
    Step("HIL-D003", "destructive-setup", "clear_vlf", ("voltage-low flag clear terminal status: OK",), timeout_s=5.0),
    Step("HIL-D004", "destructive-setup", "clear_bsf", ("backup flag clear terminal status: OK",), timeout_s=5.0),
    Step(
        "HIL-D005",
        "destructive-setup",
        "primary-cell ensure CONFIRM-PRIMARY-CELL",
        (
            "Primary-cell ensure:",
            "status=OK",
            "cleanup=verified",
            "persistent_target_verified=yes",
            "active_target_verified=yes",
        ),
        timeout_s=10.0,
    ),
    Step(
        "HIL-D005A",
        "destructive-setup",
        "primary-cell ensure CONFIRM-PRIMARY-CELL",
        ("status=PRIMARY_CELL_ALREADY_ATTEMPTED", "write_attempted=no"),
        timeout_s=5.0,
        notes="Same-lifecycle admission latch must reject a second attempt.",
    ),
    Step(
        "HIL-D005B",
        "destructive-setup",
        "backup",
        (
            "Battery Backup",
            "Backup mode: level",
            "Trickle charger bits: 0x0 (off)",
        ),
        timeout_s=5.0,
        notes="Primary-cell active readback: level switching with charging disabled.",
    ),
    Step(
        "HIL-D005C",
        "destructive-setup",
        "backup level",
        (
            "backup configuration terminal status: OK",
            "final=REQUESTED_VERIFIED",
            "mutation_attempted=no",
        ),
        timeout_s=5.0,
        notes="Already-selected level mode must complete without mutation.",
    ),
    Step(
        "HIL-D005D",
        "destructive-setup",
        "backup off",
        (
            "backup configuration terminal status: OK",
            "final=REQUESTED_VERIFIED",
            "mutation_attempted=yes",
        ),
        timeout_s=5.0,
        notes="VDD remains applied; persistent C0 is unchanged.",
    ),
    Step(
        "HIL-D005E",
        "destructive-setup",
        "backup",
        ("Backup mode: off", "Trickle charger bits: 0x0 (off)"),
        timeout_s=5.0,
        notes="Active off-mode readback with charging still disabled.",
    ),
    Step(
        "HIL-D005F",
        "destructive-setup",
        "backup level",
        (
            "backup configuration terminal status: OK",
            "final=REQUESTED_VERIFIED",
            "mutation_attempted=yes",
        ),
        timeout_s=5.0,
        notes="Restore the safe active primary-cell level mode.",
    ),
    Step(
        "HIL-D005G",
        "destructive-setup",
        "backup",
        ("Backup mode: level", "Trickle charger bits: 0x0 (off)"),
        timeout_s=5.0,
        notes="Restored level-mode readback with charging disabled.",
    ),
    Step("HIL-D006", "destructive-setup", "alarm_int 0", ("alarm interrupt terminal status: OK",), timeout_s=5.0),
    Step("HIL-D007", "destructive-setup", "timer 1 2 0", ("timer configuration terminal status: OK",), timeout_s=5.0),
    Step("HIL-D008", "destructive-setup", "clkout 0", ("CLKOUT disable terminal status: OK",), timeout_s=5.0),
    Step("HIL-D009", "destructive-setup", "evi edge 0", ("EVI edge terminal status: OK",), timeout_s=5.0),
    Step("HIL-D010", "destructive-setup", "evi debounce 0", ("EVI debounce terminal status: OK",), timeout_s=5.0),
    Step("HIL-D011", "destructive-setup", "evi overwrite 0", ("EVI overwrite terminal status: OK",), timeout_s=5.0),
    Step("HIL-D012", "destructive-setup", "offset 0.0", ("frequency offset terminal status: OK",), timeout_s=5.0),
    Step("HIL-D013", "destructive-setup", "ram_write 0 165 90 0 255", ("User RAM write of 4 byte(s)", "completed"), timeout_s=5.0),
    Step("HIL-D014", "destructive-setup", "ram 0 4", ("User RAM offset 0 len 4",), timeout_s=5.0, expected_data=bytes((165, 90, 0, 255))),
    Step(
        "HIL-D015",
        "destructive-setup",
        "backup",
        ("Backup mode: level", "Trickle charger bits: 0x0 (off)"),
        timeout_s=5.0,
        notes="Primary-cell profile remained intact after the destructive setup mix.",
    ),
)

INTENSIVE_SOAK_COMMANDS: tuple[Step, ...] = (
    Step("ISOAK-time", "intensive-soak", "time", ("Current time",), timeout_s=5.0, allow_unknown=True),
    Step("ISOAK-unix", "intensive-soak", "unix", ("Unix timestamp",), timeout_s=5.0, allow_unknown=True),
    Step("ISOAK-temp", "intensive-soak", "temp", ("Temperature",), timeout_s=5.0, allow_unknown=True),
    Step("ISOAK-status", "intensive-soak", "status", ("Status register",), timeout_s=5.0),
    Step("ISOAK-statusf", "intensive-soak", "statusf", ("Status flags",), timeout_s=5.0),
    Step("ISOAK-validity", "intensive-soak", "validity", ("PORF:", "VLF:", "BSF:"), timeout_s=5.0),
    Step("ISOAK-probe", "intensive-soak", "probe", ("Probe OK",), timeout_s=5.0),
    Step("ISOAK-drv", "intensive-soak", "drv", ("Driver Health", "State:"), timeout_s=5.0, require_ready_health=True),
    Step("ISOAK-backup", "intensive-soak", "backup", ("Battery Backup",), timeout_s=5.0),
    Step("ISOAK-ramw0", "intensive-soak", "ram_write 0 85 170 17 34", ("User RAM write of 4 byte(s)", "completed"), timeout_s=5.0),
    Step("ISOAK-ramr0", "intensive-soak", "ram 0 4", ("User RAM offset 0 len 4",), timeout_s=5.0, expected_data=bytes((85, 170, 17, 34))),
    Step("ISOAK-ramw4", "intensive-soak", "ram_write 4 51 68 204 221", ("User RAM write of 4 byte(s)", "completed"), timeout_s=5.0),
    Step("ISOAK-ramr4", "intensive-soak", "ram 4 4", ("User RAM offset 4 len 4",), timeout_s=5.0, expected_data=bytes((51, 68, 204, 221))),
    Step("ISOAK-reg", "intensive-soak", "reg 0x0D", ("reg[0x0D]",), timeout_s=5.0),
    Step("ISOAK-alarm", "intensive-soak", "alarm", ("Alarm time",), timeout_s=5.0, allow_unknown=True),
    Step("ISOAK-alarm-set", "intensive-soak", "alarm_set 30 15 10", ("alarm time terminal status: OK",), timeout_s=5.0),
    Step("ISOAK-alarm-match", "intensive-soak", "alarm_match 1 1 0", ("alarm match terminal status: OK",), timeout_s=5.0),
    Step("ISOAK-alarm-int-on", "intensive-soak", "alarm_int 1", ("alarm interrupt terminal status: OK",), timeout_s=5.0),
    Step("ISOAK-alarm-clear", "intensive-soak", "alarm_clear", ("alarm flag clear terminal status: OK",), timeout_s=5.0),
    Step("ISOAK-alarm-int-off", "intensive-soak", "alarm_int 0", ("alarm interrupt terminal status: OK",), timeout_s=5.0),
    Step("ISOAK-timer-on", "intensive-soak", "timer 64 2 1", ("timer configuration terminal status: OK",), timeout_s=5.0),
    Step("ISOAK-timer-read", "intensive-soak", "timer", ("Timer:",), timeout_s=5.0),
    Step("ISOAK-timer-off", "intensive-soak", "timer 1 2 0", ("timer configuration terminal status: OK",), timeout_s=5.0),
    Step("ISOAK-clkout-on", "intensive-soak", "clkout 1", ("CLKOUT enable terminal status: OK",), timeout_s=5.0),
    Step("ISOAK-clkout-freq", "intensive-soak", "clkout_freq 3", ("CLKOUT configuration terminal status: OK",), timeout_s=5.0),
    Step("ISOAK-clkout-off", "intensive-soak", "clkout 0", ("CLKOUT disable terminal status: OK",), timeout_s=5.0),
    Step("ISOAK-offset-set", "intensive-soak", "offset 0.0", ("frequency offset terminal status: OK",), timeout_s=5.0),
    Step("ISOAK-offset-read", "intensive-soak", "offset", ("Frequency offset",), timeout_s=5.0),
    Step("ISOAK-evi-edge", "intensive-soak", "evi edge 1", ("EVI edge terminal status: OK",), timeout_s=5.0),
    Step("ISOAK-evi-debounce", "intensive-soak", "evi debounce 3", ("EVI debounce terminal status: OK",), timeout_s=5.0),
    Step("ISOAK-evi-overwrite", "intensive-soak", "evi overwrite 1", ("EVI overwrite terminal status: OK",), timeout_s=5.0),
    Step("ISOAK-evi-read", "intensive-soak", "evi", ("EVI:",), timeout_s=5.0),
    Step("ISOAK-status-clear", "intensive-soak", "status_clear 15", ("Status flag clear terminal status: OK",), timeout_s=5.0),
    Step("ISOAK-eeprom", "intensive-soak", "eeprom", ("EEPROM",), timeout_s=20.0, allow_unknown=True),
    Step("ISOAK-stress", "intensive-soak", "stress 25", ("OK", "FAIL: 0"), timeout_s=30.0),
    Step("ISOAK-mix", "intensive-soak", "stress_mix 25", ("Mixed Operations Stress Test", "FAIL=0"), timeout_s=30.0),
    Step("ISOAK-selftest", "intensive-soak", "selftest", ("Selftest result:", "fail=0"), timeout_s=15.0),
)


def strip_ansi(text: str) -> str:
    return ANSI_RE.sub("", text).replace("\r\n", "\n").replace("\r", "\n")


def has_prompt(text: str) -> bool:
    clean = strip_ansi(text)
    return clean == PROMPT or clean.endswith("\n" + PROMPT) or clean.endswith("\r" + PROMPT)


def one_line(text: str, limit: int = 180) -> str:
    cleaned = " ".join(strip_ansi(text).split())
    if len(cleaned) <= limit:
        return cleaned
    return cleaned[: limit - 3] + "..."


def output_failure_reason(clean: str) -> str | None:
    if any(token in clean for token in FAILURE_PATTERNS):
        return "failure token found"
    if NONZERO_FAILURE_RE.search(clean):
        return "nonzero failure count found"
    return None


def classify_output(
    text: str,
    expected: Iterable[str],
    allow_unknown: bool = False,
    expected_error_tokens: Iterable[str] = (),
) -> tuple[str, str]:
    clean = strip_ansi(text)
    if not has_prompt(clean):
        return "FAIL", "missing terminal prompt; command completion is ambiguous"
    # Only an exact expected error line is exempt. A substring must not hide
    # a second error on the same line, or an unrelated failure elsewhere.
    expected_errors = {token for token in expected_error_tokens if token}
    matched_errors: list[str] = []
    remaining: list[str] = []
    for line in clean.splitlines():
        message = line.strip()
        if message.startswith("[E] "):
            message = message[4:]
        if message in expected_errors:
            matched_errors.append(message)
        else:
            remaining.append(line)
    failure_reason = output_failure_reason("\n".join(remaining))
    if failure_reason is not None:
        return "FAIL", failure_reason
    if matched_errors:
        return "UNKNOWN", "expected fixture-limited error: " + ", ".join(matched_errors)
    missing = [token for token in expected if token and token not in clean]
    if missing:
        return ("UNKNOWN" if allow_unknown else "FAIL"), "missing expected token(s): " + ", ".join(missing)
    return "PASS", ""


def read_until_prompt(
    port: serial.Serial,
    hard_timeout_s: float,
    on_chunk: Callable[[bytes], None] | None = None,
) -> str:
    start = time.monotonic()
    chunks: list[bytes] = []
    while True:
        now = time.monotonic()
        waiting = port.in_waiting
        if waiting:
            chunk = port.read(waiting)
            if on_chunk is not None:
                on_chunk(chunk)
            chunks.append(chunk)
            text = b"".join(chunks).decode("utf-8", errors="replace")
            if has_prompt(text):
                return text
        if now - start >= hard_timeout_s:
            return b"".join(chunks).decode("utf-8", errors="replace")
        time.sleep(0.02)


def response_payload_reason(command: str, text: str, expected_data: bytes | None = None) -> str | None:
    """A prompt proves framing, not delivery of data or diagnostic results."""
    args = command.split()
    clean = strip_ansi(text)
    def exactly(pattern: str):
        found = list(re.finditer(pattern, clean, re.MULTILINE))
        return found[0] if len(found) == 1 else None
    if args and args[0] == "selftest":
        result = exactly(r"^Selftest result: pass=(\d+) fail=(\d+) skip=(\d+)\s*$")
        return None if result and int(result[1]) > 0 and int(result[2]) == 0 else "incomplete or failed selftest summary"
    if args and args[0] in ("stress", "stress_mix"):
        count = int(args[1], 0)
        mixed = args[0] == "stress_mix"
        pattern = r"^Total: OK=(\d+), FAIL=(\d+) " if mixed else r"^OK: (\d+), FAIL: (\d+) "
        result = exactly(pattern)
        if not result or int(result[1]) != count or int(result[2]) != 0:
            return "incomplete or failed stress count"
        state = exactly(r"^Driver state: (?:\w+ -> )?(\w+)\s*$")
        consecutive = exactly(r"^Consecutive failures: (\d+)\s*$")
        if not state or state[1] != "READY" or not consecutive or int(consecutive[1]) != 0:
            return "missing or unhealthy stress result"
        if mixed:
            success = exactly(r"^Success delta: \+(\d+) \(ops had (\d+) OK results\)\s*$")
            failure = exactly(r"^Failure delta: \+(\d+) \(ops had (\d+) FAIL results\)\s*$")
            valid = bool(success and int(success[1]) >= count and int(success[2]) == count
                         and failure and int(failure[1]) == int(failure[2]) == 0)
        else:
            success = exactly(r"^Total success: (\d+) -> (\d+) \(expected (\d+)\) OK\s*$")
            failure = exactly(r"^Total failures: (\d+) -> (\d+) \(expected (\d+)\) OK\s*$")
            valid = bool(success and int(success[1]) + count == int(success[2]) == int(success[3])
                         and failure and int(failure[1]) == int(failure[2]) == int(failure[3]) == 0)
        return None if valid else "incomplete or inconsistent stress health counters"
    if not args or args[0] not in ("ram", "reg", "eeprom"):
        return None
    if args[0] == "reg":
        values = re.findall(r"reg\[0x([0-9A-Fa-f]{2})\] = 0x([0-9A-Fa-f]{2})\b", clean)
        return None if len(values) == 1 and int(values[0][0], 16) == int(args[1], 0) else "missing or ambiguous register value"
    base, length = (int(args[1], 0), int(args[2], 0)) if args[0] == "ram" else (0xcb, 32)
    received = bytearray()
    for line in clean.splitlines():
        match = re.fullmatch(r"\s*0x([0-9A-Fa-f]{2}):\s*((?:[0-9A-Fa-f]{2}\s*)+)\s*", line)
        if match:
            data = bytes.fromhex(match[2])
            if int(match[1], 16) != base + len(received) or not 1 <= len(data) <= 8:
                return "non-contiguous or malformed memory payload"
            received.extend(data)
    if len(received) != length:
        return "incomplete or oversized memory payload"
    if expected_data is not None and received != expected_data:
        return "memory readback differs from the requested test pattern"
    return None


def healthy_driver_reason(text: str) -> str | None:
    """Require the complete cmd_drv snapshot for a fresh, fault-free lifecycle.

    A terminal prompt does not prove that USB delivered the middle of a reply.
    Match every nonblank row in source order, including duplicate-free sections
    and both EEPROM summaries. This repository ships only the Arduino CLI.
    """
    rows = [line.strip() for line in strip_ansi(text).splitlines() if line.strip()]
    if rows and rows[0] == "drv":
        rows.pop(0)
    if rows and rows[-1] == ">":
        rows.pop()
    fields = (
        r"=== Driver Health ===",
        r"State: READY",
        r"isInitialized: yes",
        r"Config: addr=0x51 i2cTimeout=(?P<i2c_timeout>[0-9]{1,10}) offlineThreshold=(?P<threshold>[0-9]{1,3}) nowMs=(?:yes|no)",
        r"EEPROM: busy=no generic_persistence=(?:yes|no) timeout=(?P<eeprom_timeout>[0-9]{1,10}) queue=0 queue_ok=(?P<queue_ok>[0-9]{1,10}) queue_fail=0",
        r"=== Counters ===",
        r"Consecutive Failures: 0",
        r"Total Successes: (?P<success>[0-9]{1,10})",
        r"Total Failures: 0",
        r"Success rate: (?P<rate>0\.0|100\.0)%",
        r"=== Timestamps ===",
        r"Last OK: (?:never|(?P<ok_age>[0-9]{1,10}) ms ago \(at (?P<ok_at>[1-9][0-9]{0,9}) ms\))",
        r"Last Error: never",
        r"=== Last Error Details ===",
        r"Code: OK \(0\)",
        r"Detail: 0",
        r"Message: OK",
        r"=== EEPROM State ===",
        r"Busy: false",
        r"Status: OK",
    )
    if len(rows) != len(fields):
        return "incomplete or ambiguous driver health snapshot"
    values: dict[str, str] = {}
    for row, pattern in zip(rows, fields):
        found = re.fullmatch(pattern, row)
        if found is None:
            return "malformed, misplaced or unhealthy driver field: " + row
        values.update({name: value for name, value in found.groupdict().items() if value is not None})
    for name, value in values.items():
        if name != "rate" and int(value) > 0xFFFFFFFF:
            return "out-of-range driver health field: " + name
    if not 1 <= int(values["threshold"]) <= 255:
        return "out-of-range driver offline threshold"
    if not 1 <= int(values["i2c_timeout"]) <= 100:
        return "out-of-range driver I2C timeout"
    if not 10 <= int(values["eeprom_timeout"]) <= 250:
        return "out-of-range driver EEPROM timeout"
    expected_rate = "100.0" if int(values["success"]) else "0.0"
    if values["rate"] != expected_rate:
        return "driver success rate disagrees with complete counters"
    return None


def read_command_response(
    port: serial.Serial,
    idle_timeout_s: float,
    hard_timeout_s: float,
    expected: Iterable[str],
    expected_error_tokens: Iterable[str] = (),
    on_chunk: Callable[[bytes], None] | None = None,
) -> str:
    # Keep the old arguments for host callers, but never inject a newline to
    # manufacture completion after idle or timeout. Only the CLI prompt frames
    # a completed command; a timeout leaves the session unusable.
    return read_until_prompt(port, hard_timeout_s, on_chunk)


class HilSession:
    def __init__(
        self,
        port_name: str,
        baud: int,
        command_gap_s: float,
        default_idle_timeout_s: float,
        verbose: bool,
        transcript_path: Path | None = None,
    ) -> None:
        self.port_name = port_name
        self.baud = baud
        self.command_gap_s = command_gap_s
        self.default_idle_timeout_s = default_idle_timeout_s
        self.verbose = verbose
        self.serial: serial.Serial | None = None
        self.transcript: list[str] = []
        self.transcript_path = transcript_path
        self.transcript_file = None
        self.framing_ok = False

    def __enter__(self) -> "HilSession":
        if self.transcript_path is not None:
            self.transcript_path.parent.mkdir(parents=True, exist_ok=True)
            self.transcript_file = self.transcript_path.open("xb")
        try:
            self.serial = serial.Serial(
                None,
                self.baud,
                timeout=0.05,
                write_timeout=1.0,
                rtscts=False,
                dsrdtr=False,
            )
            self.serial.setDTR(True)
            self.serial.setRTS(False)
            self.serial.port = self.port_name
            self.serial.open()
        except Exception:
            self.__exit__()
            raise
        return self

    def __exit__(self, *_args: object) -> None:
        try:
            if self.serial is not None:
                self.serial.close()
        finally:
            if self.transcript_file is not None:
                self.transcript_file.close()

    def capture_boot(self, settle_s: float, timeout_s: float) -> tuple[str, float]:
        assert self.serial is not None
        self._record("", "")
        start = time.monotonic()
        if settle_s > 0:
            time.sleep(settle_s)
        text = read_until_prompt(self.serial, timeout_s, self._capture_chunk)
        self.framing_ok = has_prompt(text)
        elapsed = time.monotonic() - start
        return text, elapsed

    def run_step(self, step: Step) -> Result:
        assert self.serial is not None
        if not self.framing_ok:
            raise RuntimeError("serial framing lost; refusing further commands")
        self.framing_ok = False
        self._record(step.command, "")
        idle = step.idle_timeout_s if step.idle_timeout_s is not None else self.default_idle_timeout_s
        start = time.monotonic()
        try:
            if step.command:
                if self.verbose:
                    print(f"[HIL] {step.test_id}: {step.command}", flush=True)
                payload = (step.command + "\n").encode("utf-8")
                if self.serial.write(payload) != len(payload):
                    raise RuntimeError("short serial command write; refusing further commands")
            text = read_command_response(
                self.serial, idle, step.timeout_s, step.expected,
                step.expected_error_tokens, self._capture_chunk,
            )
            self.framing_ok = has_prompt(text)
            status, notes = classify_output(text, step.expected, step.allow_unknown, step.expected_error_tokens)
            if status != "FAIL":
                reason = response_payload_reason(step.command, text, step.expected_data)
                if reason is not None:
                    status, notes = "FAIL", reason
            if status != "FAIL" and step.require_ready_health:
                reason = healthy_driver_reason(text)
                if reason is not None:
                    status, notes = "FAIL", reason
            if "ESP-ROM:" in text:
                self.framing_ok = False
                status, notes = "FAIL", "device rebooted during command; refusing further commands"
        except (OSError, RuntimeError) as exc:
            text = ""
            status, notes = "FAIL", f"serial operation failed: {exc}; refusing further commands"
            self._capture_chunk(("\n[HOST] " + notes + "\n").encode("utf-8"))
        elapsed = time.monotonic() - start
        if step.notes:
            notes = (notes + "; " if notes else "") + step.notes
        if self.command_gap_s > 0:
            time.sleep(self.command_gap_s)
        return Result(
            test_id=step.test_id,
            area=step.area,
            command=step.command or "(boot transcript)",
            expected=", ".join(step.expected),
            observed=one_line(text),
            elapsed_s=elapsed,
            status=status,
            notes=notes,
        )

    def _record(self, command: str, text: str) -> None:
        stamp = _dt.datetime.now().isoformat(timespec="seconds")
        if command:
            header = f"\n===== {stamp} COMMAND: {command} =====\n"
        else:
            header = f"\n===== {stamp} BOOT/IDLE CAPTURE =====\n"
        self._capture_chunk((header + text).encode("utf-8"))

    def _capture_chunk(self, chunk: bytes) -> None:
        if self.transcript_file is not None:
            self.transcript_file.write(chunk)
            self.transcript_file.flush()
        else:
            self.transcript.append(chunk.decode("utf-8", errors="replace"))


def markdown_table(results: Iterable[Result]) -> str:
    rows = [
        "| Test ID | Area | Command | Expected | Observed | Elapsed s | Status | Notes |",
        "|---|---|---|---|---|---:|---|---|",
    ]
    for res in results:
        rows.append(
            "| {test_id} | {area} | `{command}` | {expected} | {observed} | {elapsed:.3f} | {status} | {notes} |".format(
                test_id=escape_md(res.test_id),
                area=escape_md(res.area),
                command=escape_md(res.command),
                expected=escape_md(res.expected),
                observed=escape_md(res.observed),
                elapsed=res.elapsed_s,
                status=escape_md(res.status),
                notes=escape_md(res.notes),
            )
        )
    return "\n".join(rows) + "\n"


def escape_md(text: str) -> str:
    return text.replace("|", "\\|").replace("\n", " ")


def summarize(results: Iterable[Result]) -> dict[str, int]:
    summary = {"PASS": 0, "FAIL": 0, "UNKNOWN": 0, "NOT RUN": 0}
    for res in results:
        summary[res.status] = summary.get(res.status, 0) + 1
    return summary


def run_parser_self_test() -> int:
    cases = (
        ("ok", "Probe OK\nHealth tracking: unchanged\n> ", ("Probe OK",), False, "PASS"),
        ("missing", "Temperature: 21.5 C\n> ", ("Current RTC time",), True, "UNKNOWN"),
        ("failure", "[E] Probe FAILED: I2C timeout\n> ", ("Probe OK",), False, "FAIL"),
        ("nonzero-failure-count", "Selftest result: pass=11 FAIL=1 skip=0\n> ",
         ("Selftest result:",), False, "FAIL"),
        ("selftest", "[PASS] readTime\nSelftest result: pass=12 fail=0 skip=0\n> ", ("Selftest result:", "fail=0"), False, "PASS"),
        ("ram-acceptance-only", "[I] User RAM write accepted\n> ",
         ("user RAM write terminal status: OK",), False, "FAIL"),
    )
    failed = 0
    for name, text, expected, allow_unknown, want in cases:
        got, _ = classify_output(text, expected, allow_unknown)
        if got != want:
            print(f"parser-self-test {name}: got {got}, expected {want}")
            failed += 1
    missing_authorization = argparse.Namespace(
        destructive_setup=True,
        port="COM99",
        authorization_port=None,
        authorization_module=None,
        authorization_primary_cell_chemistry=None,
        authorization_power_conditions=None,
        authorization_c0_write=None,
        authorization_vdd_off_backfeed_scope=None,
    )
    try:
        destructive_authorization_record(missing_authorization)
        print("parser-self-test authorization-missing: unexpectedly accepted")
        failed += 1
    except SystemExit:
        pass
    valid_authorization = argparse.Namespace(
        destructive_setup=True,
        port="COM99",
        authorization_port="COM99",
        authorization_module="fixture-a",
        authorization_primary_cell_chemistry="non-rechargeable Li-MnO2",
        authorization_power_conditions="VDD 3.3 V, VBACKUP measured",
        authorization_c0_write="CONFIRM-POSSIBLE-C0-WRITE",
        authorization_vdd_off_backfeed_scope="no VDD-off step authorized",
    )
    record = destructive_authorization_record(valid_authorization)
    if "possible_c0_write=authorized" not in record or "port=COM99" not in record:
        print("parser-self-test authorization-valid: evidence record incomplete")
        failed += 1
    if failed:
        return 1
    print("parser-self-test PASSED")
    return 0


def dry_run(args: argparse.Namespace) -> int:
    steps = list(SAFE_FUNCTIONAL_STEPS)
    if args.destructive_setup:
        steps.extend(INTENSIVE_SETUP_STEPS)
        print(
            "Destructive hardware execution additionally requires the full "
            "fresh --authorization-* scope; dry-run opens no serial port."
        )
    if args.include_stress:
        steps.extend(STRESS_STEPS)
    print(f"Dry run: {len(steps)} planned functional step(s)")
    for step in steps:
        print(f"{step.test_id}: {step.area}: {step.command or '(boot transcript)'}")
    if args.soak_duration_s > 0:
        commands = INTENSIVE_SOAK_COMMANDS if args.intensive_soak else SOAK_COMMANDS
        print(f"Soak enabled for {args.soak_duration_s} second(s); command mix:")
        for step in commands:
            print(f"  {step.command}")
    return 0


def run_observed_step(session: HilSession, step: Step) -> list[Result]:
    """Keep health adjacent to stress and failures, while framing is intact."""
    results: list[Result] = []
    stress = step.command.startswith("stress") or step.command == "selftest"
    health = Step(step.test_id + "-health-before", "health", "drv", ("Driver Health", "State:"), require_ready_health=True)
    if stress:
        before = session.run_step(health)
        results.append(before)
        if before.status == "FAIL":
            return results
    result = session.run_step(step)
    results.append(result)
    if session.framing_ok and (stress or result.status == "FAIL") and step.command != "drv":
        # A follow-up after failure is an observation, not an assertion that
        # the failed driver should already have recovered.
        results.append(session.run_step(replace(
            health, test_id=step.test_id + "-health-after",
            require_ready_health=result.status != "FAIL",
        )))
    return results


def run_soak(
    session: HilSession,
    commands: tuple[Step, ...],
    duration_s: float,
    progress_interval_s: float,
) -> tuple[list[Result], SoakStats]:
    stats = SoakStats(start=_dt.datetime.now().isoformat(timespec="seconds"))
    results: list[Result] = []
    if duration_s <= 0:
        stats.end = stats.start
        return results, stats

    deadline = time.monotonic() + duration_s
    next_progress = time.monotonic() + max(1.0, progress_interval_s)
    consecutive_failures = 0
    index = 0
    while time.monotonic() < deadline:
        template = commands[index % len(commands)]
        if deadline - time.monotonic() < template.timeout_s + 1.0:
            break
        index += 1
        step = replace(template, test_id=f"SOAK-{index:06d}")
        observed = run_observed_step(session, step)
        results.extend(observed)
        for res in observed:
            stats.command_counts[res.command] = stats.command_counts.get(res.command, 0) + 1
            stats.latencies_s.append(res.elapsed_s)
            if res.elapsed_s > stats.worst_latency_s:
                stats.worst_latency_s = res.elapsed_s
                stats.worst_command = res.command
            if res.status == "PASS":
                stats.pass_count += 1
                consecutive_failures = 0
            elif res.status == "UNKNOWN":
                stats.unknown_count += 1
                consecutive_failures = 0
            elif res.status == "FAIL":
                stats.fail_count += 1
                consecutive_failures += 1
                stats.consecutive_failures_max = max(stats.consecutive_failures_max, consecutive_failures)
            else:
                stats.not_run_count += 1
        if any(res.status == "FAIL" for res in observed):
            break
        now = time.monotonic()
        if progress_interval_s > 0 and now >= next_progress:
            elapsed = duration_s - max(0.0, deadline - now)
            print(
                "SOAK progress: elapsed={:.0f}s pass={} fail={} unknown={} worst={:.3f}s command={}".format(
                    elapsed,
                    stats.pass_count,
                    stats.fail_count,
                    stats.unknown_count,
                    stats.worst_latency_s,
                    stats.worst_command,
                ),
                flush=True,
            )
            next_progress = now + progress_interval_s
    stats.end = _dt.datetime.now().isoformat(timespec="seconds")
    stats.duration_s = duration_s - max(0.0, deadline - time.monotonic())
    return results, stats


def write_artifacts(
    transcript_path: Path | None,
    markdown_path: Path | None,
    json_path: Path | None,
    transcript: str,
    results: list[Result],
    soak_stats: SoakStats | None,
) -> None:
    if transcript_path:
        transcript_path.parent.mkdir(parents=True, exist_ok=True)
        transcript_path.write_text(transcript, encoding="utf-8")
    if markdown_path:
        markdown_path.parent.mkdir(parents=True, exist_ok=True)
        body = ["# RV3032-C7 HIL Runner Results", "", markdown_table(results)]
        if soak_stats is not None:
            body.extend(
                [
                    "",
                    "## Soak Summary",
                    "",
                    f"- Start: {soak_stats.start}",
                    f"- End: {soak_stats.end}",
                    f"- Duration: {soak_stats.duration_s:.1f} s",
                    f"- Pass/Fail/Unknown/Not Run: {soak_stats.pass_count}/{soak_stats.fail_count}/{soak_stats.unknown_count}/{soak_stats.not_run_count}",
                    f"- Worst latency: {soak_stats.worst_latency_s:.3f} s (`{soak_stats.worst_command}`)",
                    f"- Command counts: `{json.dumps(soak_stats.command_counts, sort_keys=True)}`",
                ]
            )
        markdown_path.write_text("\n".join(body) + "\n", encoding="utf-8")
    if json_path:
        json_path.parent.mkdir(parents=True, exist_ok=True)
        payload = {
            "results": [res.__dict__ for res in results],
            "summary": summarize(results),
            "soak": soak_stats.__dict__ if soak_stats else None,
        }
        json_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def destructive_authorization_record(args: argparse.Namespace) -> str:
    if not args.destructive_setup:
        return ""
    required = {
        "--authorization-port": args.authorization_port,
        "--authorization-module": args.authorization_module,
        "--authorization-primary-cell-chemistry":
            args.authorization_primary_cell_chemistry,
        "--authorization-power-conditions": args.authorization_power_conditions,
        "--authorization-c0-write": args.authorization_c0_write,
        "--authorization-vdd-off-backfeed-scope":
            args.authorization_vdd_off_backfeed_scope,
    }
    missing = [name for name, value in required.items() if not value]
    if missing:
        raise SystemExit(
            "destructive setup requires fresh authorization fields: "
            + ", ".join(missing)
        )
    if args.authorization_port != args.port:
        raise SystemExit(
            "--authorization-port must exactly match the selected --port"
        )
    if args.authorization_c0_write != "CONFIRM-POSSIBLE-C0-WRITE":
        raise SystemExit(
            "destructive setup requires "
            "--authorization-c0-write CONFIRM-POSSIBLE-C0-WRITE"
        )
    return (
        f"port={args.authorization_port}; module={args.authorization_module}; "
        f"primary_cell_chemistry={args.authorization_primary_cell_chemistry}; "
        f"power_conditions={args.authorization_power_conditions}; "
        "possible_c0_write=authorized; "
        f"vdd_off_backfeed_scope={args.authorization_vdd_off_backfeed_scope}"
    )


def run_hardware(args: argparse.Namespace) -> int:
    if not args.port:
        raise SystemExit("hardware HIL requires an explicit --port")
    global serial
    try:
        import serial
    except ImportError as exc:  # pragma: no cover - exercised by operator env
        raise SystemExit(
            "hardware HIL requires pyserial: python -m pip install pyserial"
        ) from exc

    authorization = destructive_authorization_record(args)
    results: list[Result] = []
    if authorization:
        results.append(
            Result(
                "HIL-AUTH",
                "authorization",
                "operator authorization",
                "fresh destructive scope captured",
                authorization,
                0.0,
                "PASS",
                "Required before opening the selected serial port.",
            )
        )
    soak_stats: SoakStats | None = None
    transcript_path = Path(args.transcript_out) if args.transcript_out else (
        Path(".pio/hil-runs") / (_dt.datetime.now().strftime("%Y%m%d-%H%M%S-%f") + "-" + args.port + ".log")
    )
    session = HilSession(
        args.port,
        args.baud,
        command_gap_s=args.command_gap_ms / 1000.0,
        default_idle_timeout_s=args.idle_timeout_s,
        verbose=args.verbose,
        transcript_path=transcript_path,
    )
    try:
        with session:
            boot_text, boot_elapsed = session.capture_boot(args.boot_settle_s, args.boot_timeout_s)
            boot_step = SAFE_FUNCTIONAL_STEPS[0]
            boot_status, boot_notes = classify_output(
                boot_text,
                boot_step.expected,
                allow_unknown=True,
                expected_error_tokens=boot_step.expected_error_tokens,
            )
            results.append(Result(
                boot_step.test_id, boot_step.area, "(boot transcript)",
                ", ".join(boot_step.expected), one_line(boot_text), boot_elapsed,
                boot_status, boot_notes,
            ))
            steps = list(SAFE_FUNCTIONAL_STEPS[1:])
            if args.destructive_setup:
                steps.extend(INTENSIVE_SETUP_STEPS)
            if args.include_stress:
                steps.extend(STRESS_STEPS)
            for step in steps:
                if any(res.status == "FAIL" for res in results):
                    break
                results.extend(run_observed_step(session, step))
            if args.soak_duration_s > 0 and not any(res.status == "FAIL" for res in results):
                commands = INTENSIVE_SOAK_COMMANDS if args.intensive_soak else SOAK_COMMANDS
                soak_results, soak_stats = run_soak(session, commands, args.soak_duration_s, args.progress_interval_s)
                results.extend(soak_results)
            if session.framing_ok and not any(res.status == "FAIL" for res in results):
                results.append(session.run_step(Step("HIL-final-health", "health", "drv", ("Driver Health", "State:"), require_ready_health=True)))
    except (Exception, KeyboardInterrupt) as exc:
        results.append(Result("HIL-host", "host", "(runner)", "completed run",
                              str(exc), 0.0, "FAIL", "Run stopped; live transcript retained."))
    finally:
        # The transcript was flushed on receipt, including partial responses.
        # Never rewrite/truncate it while generating condensed final results.
        write_artifacts(
            None,
            Path(args.markdown_out) if args.markdown_out else None,
            Path(args.json_out) if args.json_out else None,
            "", results, soak_stats,
        )

    summary = summarize(results)
    latencies = [r.elapsed_s for r in results if r.status in ("PASS", "UNKNOWN")]
    print(f"HIL summary: {summary}")
    if latencies:
        print(
            "Latency s: min={:.3f} mean={:.3f} max={:.3f}".format(
                min(latencies), statistics.fmean(latencies), max(latencies)
            )
        )
    if args.markdown_out:
        print(f"Markdown results: {args.markdown_out}")
    print(f"Transcript: {transcript_path}")
    return 1 if summary.get("FAIL", 0) else 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--port",
        help="serial port for physical HIL (required outside device-free modes)",
    )
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--idle-timeout-s", type=float, default=0.35)
    parser.add_argument("--boot-timeout-s", type=float, default=8.0)
    parser.add_argument("--boot-settle-s", type=float, default=1.0)
    parser.add_argument("--command-gap-ms", type=int, default=100)
    parser.add_argument("--include-stress", action="store_true")
    parser.add_argument("--destructive-setup", action="store_true")
    parser.add_argument("--authorization-port")
    parser.add_argument("--authorization-module")
    parser.add_argument("--authorization-primary-cell-chemistry")
    parser.add_argument("--authorization-power-conditions")
    parser.add_argument("--authorization-c0-write")
    parser.add_argument("--authorization-vdd-off-backfeed-scope")
    parser.add_argument("--intensive-soak", action="store_true")
    parser.add_argument("--soak-duration-s", type=float, default=0.0)
    parser.add_argument("--progress-interval-s", type=float, default=60.0)
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--transcript-out")
    parser.add_argument("--markdown-out")
    parser.add_argument("--json-out")
    parser.add_argument("--parser-self-test", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.parser_self_test:
        return run_parser_self_test()
    if args.dry_run:
        return dry_run(args)
    return run_hardware(args)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
