# Variants And Open Questions

Known source distinction:

| Source | Relevance | Source |
|---|---|---|
| Short datasheet | Headline feature, dimensions, pin list, electrical summaries. | datasheet, pp. 1-2 |
| Application manual Rev. 1.3 | Detailed source for registers, I2C, interrupts, EEPROM, and application circuits. | application manual, pp. 1-154 |
| Automotive qualification | Datasheet says AEC-Q200 availability exists. Confirm ordering part before claiming automotive qualification in code/docs. | datasheet, p. 1 |

Not documented in PDFs / repository policy choices:

- The PDFs document interrupt sources and enable bits, but do not define a preferred public API subset.
- The PDFs document EEPROM persistence and password protection, but do not prescribe software confirmation policy before EEPROM writes.
- The PDFs expose `PORF` and `VLF`; they do not define a software time-valid cache policy.
- The PDFs state CLKOUT is enabled at 32.768 kHz by default delivery setting and can be disabled; they do not require automatic software disabling.
- The PDFs define register encodings, but do not define how an API should represent BCD fields or the 100th-second register.

Raw extraction remains in `docs/pdf-extracted-md` for verification.
