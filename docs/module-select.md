# SparAkashaAnanta Module Profile Selection

SparAkashaAnanta does not have module ID pins. Because of that, firmware cannot reliably identify every attached module by itself.

This repository provides a user-selected module profile instead. A keymap behavior stores the selected module profile in Zephyr settings, and the value is restored on the next boot.

## Behavior

Include the binding header:

```dts
#include <dt-bindings/saa/module_select.h>
```

Use the behavior:

```dts
&saa_mod SAA_MODULE_TB
```

Available values:

| Value | Meaning |
| ----- | ------- |
| `SAA_MODULE_UNSPECIFIED` | No profile selected |
| `SAA_MODULE_KEY` | Direct key module |
| `SAA_MODULE_ENC` | Rotary encoder module |
| `SAA_MODULE_JOY` | Joystick module |
| `SAA_MODULE_TB` | Trackball module |
| `SAA_MODULE_TPD` | Touchpad module |

IQS is not a mutually exclusive base module profile. It is treated as an optional module that can coexist with `KEY`, `ENC`, `JOY`, `TB`, and `TPD`, and is assumed to be available in builds that include the `IQS` snippet.

## What This Does

- Saves the selected module profile under `saa/module/selected`.
- Restores the selected module profile at boot.
- Exposes the current profile through `saa_module_select_get()`.
- Exposes the selected profile's required input path through `saa_module_select_get_capabilities()`.
- Runs as a global behavior so split halves can receive the same selection command.

## Module Capabilities

The selected base module maps to these input paths:

| Profile | SPI | I2C | ADC | Encoder A/B |
| ------- | --- | --- | --- | ----------- |
| `KEY` | no | no | no | no |
| `ENC` | no | no | no | yes |
| `JOY` | no | no | yes | yes |
| `TB` | yes | no | no | no |
| `TPD` | no | yes | no | no |

IQS is separate from this table. It is treated as an optional module that coexists with every base module profile.

## What This Does Not Do Yet

This does not dynamically switch Devicetree drivers. ZMK input devices are still built from snippets at compile time.

The saved profile is intended as runtime state for future filtering, UI display, diagnostics, or driver gating where the hardware definition allows it. In standard ZMK/Zephyr initialization, `settings_load()` happens after device initialization, so this saved value cannot directly decide which Devicetree devices become `okay` during boot.
