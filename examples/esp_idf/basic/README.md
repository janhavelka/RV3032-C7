# Native ESP-IDF Basic Example

Diagnostic bring-up example for the RV3032-C7 core driver under ESP-IDF.

```bash
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

Default pins are SDA GPIO8 and SCL GPIO9. Override them at compile time with
`RV3032_EXAMPLE_I2C_SDA` and `RV3032_EXAMPLE_I2C_SCL` when your board uses
different routing.

The example owns the I2C bus, device handle, mutex, timeout policy, logging, and
task scheduling. The library receives only injected transport callbacks and an
optional monotonic millisecond timebase.
