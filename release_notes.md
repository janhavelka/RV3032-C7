# RV3032-C7 v1.4.0 Release Notes
Date: 2026-04-03

## Highlights
- Added granular I2C transport status codes for address NACK, data NACK, timeout, and bus faults.
- Removed the unnecessary `Wire` dependency from library metadata; the driver remains transport-injected.
- Standardized the example `Wire` adapter around advisory per-call timeouts and application-owned bus timeout configuration.
- Refreshed README and public-header documentation so the transport contract, version exposure, and shipped docs are aligned.

## Compatibility and Migration
- Existing public APIs remain backward compatible.
- The `Err` enum is append-only in this release.

## Tag
- `v1.4.0`

## Suggested GitHub Release Title
- `RV3032-C7 v1.4.0`
