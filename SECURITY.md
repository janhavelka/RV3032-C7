# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 3.0.x   | :white_check_mark: |
| < 3.0   | :x:                |

## Reporting a Vulnerability

If you discover a security vulnerability within this library, please follow responsible disclosure:

1. **Do NOT** open a public GitHub issue.
2. Email the maintainer at: `jan@havelka.dev`.
3. Include:
   - A description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Any suggested fixes (optional)

We will acknowledge receipt within 48 hours and aim to provide a fix or mitigation within 14 days for critical issues.

## Scope

This library talks to one I2C device through application-supplied callbacks.
It performs no dynamic allocation in steady state, contains no network code,
and writes nothing outside the RV-3032-C7 itself. On-chip EEPROM persistence
is opt-in through `Config::enableEepromWrites` and is wear-limited; see the
endurance note in `docs/DEVICE_REFERENCE.md`.
