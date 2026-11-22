# RV3032-C7 native ESP-IDF CLI

This example builds with ESP-IDF 6.0.1 for ESP32-S2 and ESP32-S3. It uses the
shared RV3032 command engine and no Arduino sources. One application task owns
the console, I2C bus, driver and cooperative polling.

## Connect the RTC

| Signal or setting | Example value |
|---|---|
| SDA | GPIO8 |
| SCL | GPIO9 |
| Ground | Common ground between the ESP32 board and RTC |
| I2C clock | 400 kHz |
| RV3032 7-bit address | `0x51` |
| Ordinary transfer timeout | 50 ms |

Use external pull-ups on SDA and SCL. Size them for the bus capacitance and
use a pull-up voltage compatible with both devices. The RTC requires VDD of
at least 2.0 V at 400 kHz.
[Supply and backup topology requirements](../../../docs/DEVICE_REFERENCE.md)

## Build and open the console

Use the board's native USB connector. ESP32-S3 uses USB Serial/JTAG and
ESP32-S2 uses USB CDC through the ROM console. From the repository root in
PowerShell, select the target for your board and pass the same build folder
and sdkconfig to each invocation.

```powershell
$rtcTarget = "esp32s3"   # For ESP32-S2, use "esp32s2".
$rtcProject = "examples\esp_idf\basic"
$rtcBuild = "C:\esp\build\rv3032-$rtcTarget-6.0.1"
C:\esp\idf.cmd 6.0.1 -C $rtcProject -B $rtcBuild -D "SDKCONFIG=$rtcBuild.sdkconfig" set-target $rtcTarget build
```

To flash the designated board and start the monitor, replace `COM7` with its
native USB port.

```powershell
C:\esp\idf.cmd 6.0.1 -C $rtcProject -B $rtcBuild -D "SDKCONFIG=$rtcBuild.sdkconfig" -p COM7 flash monitor
```

At the `> ` prompt, try `help`, `version`, `probe`, `drv`, `validity`
and `time`. The boot probe checks communication with address `0x51`.
`version` shows the library's `Version.h` metadata and separate example build
information. CMake records the example's UTC build time, short commit and
whether tracked files have changes. An extracted package without its own Git checkout reports
`unknown` for the example commit and status.

## Commands that change the device

Start with the read-only commands above. Clock setters change the running
calendar, flag clears consume diagnostic evidence and register or RAM writes
can overwrite live state. The CLI discards input received while a cooperative
operation is pending. Wait for its terminal result and prompt before entering
another command.

Generic EEPROM writes are disabled. The separate
`primary-cell ensure CONFIRM-PRIMARY-CELL` command can issue one wear-limited
C0 write to configure level switching and disable charging. Run it only for
a confirmed primary cell after checking the backup topology and supply
conditions for the full write and activation interval.
[Primary-cell electrical prerequisites and verification](../../../README.md#primary-cell-configuration)

The build verifies compilation and linking. The static checker verifies the
source contracts. Physical hardware validation, on-target console I/O, HIL and the
main task's stack high-water mark are **NOT RUN**.
