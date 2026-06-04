# SparAkashaAnanta Module Auto Detection

SparAkashaAnanta now has a small Zephyr module that reads module ID strap pins at boot.

The detector is intentionally conservative: it does not enable every input driver at once, and it does not switch Devicetree nodes dynamically. ZMK and Zephyr still build concrete input devices from snippets at compile time. The detector verifies that the attached hardware class matches the firmware image.

## ID Pins

The current implementation uses two unused XIAO Plus pins:

| ID bit | XIAO pin | nRF52840 pin |
| ------ | -------- | ------------ |
| ID0 | D18 | P1.05 |
| ID1 | D19 | P1.07 |

Both pins are configured as active-low inputs with pull-ups. A strapped-to-GND bit reads as `1`; an open bit reads as `0`.

## Current Mapping

| ID1 | ID0 | Detected class |
| --- | --- | -------------- |
| 0 | 0 | `KEY` |
| 0 | 1 | `ENC` |
| 1 | 0 | `JOY` |
| 1 | 1 | `POINTING` |

`POINTING` covers trackball, Cirque touchpad, and IQS-style pointing modules because the current two-bit ID does not have enough states to distinguish every module.

## Firmware Match Check

Each module snippet sets `expected-module` on `&saa_module_detector`.

At boot the firmware logs:

```text
SAA module detected: id=<n> name=<class>
```

If the attached module class does not match the selected snippet, the firmware logs a warning:

```text
SAA module mismatch: firmware expects <class> but module ID reports <class>
```

## Limitation

Full automatic module selection needs more hardware information than the current connector definition provides. To distinguish `KEY`, `ENC`, `JOY`, `TB`, `TPD`, and `IQS` individually, use one of these approaches:

- Add a third module ID pin.
- Add an ID resistor ladder on an ADC-capable pin.
- Add a small module EEPROM or I2C ID device.

Until then, the correct module snippet is still required at build time.
