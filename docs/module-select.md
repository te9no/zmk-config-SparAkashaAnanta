# ZMK Input Module Profile Selection for SparAkashaAnanta

SparAkashaAnanta does not have module ID pins. Because of that, firmware cannot reliably identify every attached module by itself.

This repository provides a user-selected module profile instead. The reusable `zmk,input-module-mux` module stores the selected profile in Zephyr settings, and the value is restored on the next boot.

The implementation is intentionally not tied to SparAkashaAnanta. SAA provides the profile IDs, keymap binding, and `settings-key`; the reusable part is the `zmk,input-module-*` behavior, mux, capability flags, and public API.

## Reusable Model

For another keyboard, the same mechanism should be reusable with these pieces:

- Define keyboard-specific profile IDs in a `dt-bindings/<keyboard>/...` header.
- Add a `zmk,behavior-input-module-select` behavior node.
- Add a `zmk,input-module-mux` node with keyboard-specific profile child nodes.
- Set a keyboard-specific `settings-key` to avoid collisions.
- Bind the behavior in the keymap to let the user choose the next boot profile.
- Optionally attach deferred candidate devices to each profile with `devices`.

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
- Exposes the current profile through `zmk_input_module_selected_get()`.
- Exposes the selected profile's required input path through `zmk_input_module_selected_capabilities()`.
- Keeps SAA compatibility wrappers such as `saa_module_select_get()`.
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

This does not dynamically switch Devicetree nodes from `disabled` to `okay`. ZMK input devices are still described at compile time, but selected candidate devices can now be deferred and initialized after settings are loaded.

The saved profile is intended as runtime state for future filtering, UI display, diagnostics, or driver gating where the hardware definition allows it. In standard ZMK/Zephyr initialization, `settings_load()` happens after device initialization, so this saved value cannot directly decide which Devicetree devices become `okay` during boot.

The current implementation is a generic mechanism living in this repository. The next cleanup step is moving it into a standalone ZMK module so SAA, GeaconPolaris, and future keyboards can consume the same implementation without copying source files.

`config/west.yml` pins Zephyr to `te9no/zephyr` revision `feature/deferred-device-init`. That branch provides `zephyr,deferred-init`, `device_init()`, and the DTS validation support needed for non-base ZMK bindings.

## Module Mux

`zmk,input-module-mux` is the post-settings apply point. Profiles are declared as child nodes:

```dts
saa_module_mux: module_mux {
    compatible = "zmk,input-module-mux";
    settings-key = "saa/module";
    default-profile = <SAA_MODULE_UNSPECIFIED>;

    profile_trackball {
        profile-id = <SAA_MODULE_TB>;
        display-name = "TB";
        capabilities = <ZMK_INPUT_MODULE_CAP_SPI>;
        devices = <&trackball>;
    };
};
```

Each profile can be wired to deferred candidate devices through `devices`.

| Property | Profile |
| -------- | ------- |
| `profile-id` | Numeric profile selected by keymap behavior |
| `display-name` | Name used for logs/UI |
| `capabilities` | `ZMK_INPUT_MODULE_CAP_*` bitmask |
| `devices` | Deferred devices initialized when selected |

The local Zephyr deferred-init patch allows a candidate device with `zephyr,deferred-init` to be skipped during normal boot and initialized later with `device_init()`. The mux calls `device_init()` only for the selected profile's candidate devices.

## ModuleMux Snippet

`snippets/ModuleMux` is the first unified-module snippet. The normal `build.yaml` now builds this firmware path instead of per-module firmware variants.

Current candidate coverage:

| Profile | Candidate device | Deferred | Notes |
| ------- | ---------------- | -------- | ----- |
| `KEY` | `kscan2` direct GPIO kscan | yes | Shares pins with encoder/TB paths, so runtime behavior must be checked carefully. |
| `ENC` | `left_encoder` or `right_encoder` | yes | Uses the same encoder settings as the `ENC` snippet. |
| `JOY` | TODO | TODO | Needs ADC plus an encoder candidate. This requires a runtime sensor-slot strategy because JOY and ENC use different encoder step settings. |
| `TB` | TODO | TODO | Leave for later because it has stronger SPI/pin interaction. |
| `TPD` | TODO | TODO | Leave for later because the current overlay references missing `&i2c2`. |

Normal build targets:

- `SAA_L_UNIFIED`: `Central ModuleMux IQS`
- `SAA_R_UNIFIED`: `Peripheral ModuleMux IQS`

Legacy per-module targets are kept in `build.diagnostics.yaml` for comparison and debugging.

## Known Constraints

- `config/west.yml` expects `feature/deferred-device-init` to exist on `te9no/zephyr`.
- `build.yaml` is unified-firmware-first. Use `build.diagnostics.yaml` for legacy per-module diagnostics.
- `devices` are currently wired for `KEY` and `ENC` only through the `ModuleMux` snippet. IQS is included with the unified firmware and remains independent from the mutually-exclusive base module profiles.
- `JOY` cannot be completed by simply sharing the `ENC` encoder node because the existing snippets use different encoder `steps` and `triggers-per-rotation` values.
- `snippets/TPD/TPD.overlay` currently references `&i2c2`, which is not provided by the XIAO nRF52840 DTS. With OLED on I2C0 and IQS on I2C1, TPD + IQS coexistence needs an explicit I2C strategy before the one-firmware target can be completed.

## Roadmap to One Firmware

1. Move the generic `zmk,input-module-*` implementation into a standalone reusable ZMK module.
2. Publish and maintain the Zephyr deferred-init branch pinned by `west.yml`.
3. Validate `SAA_L_UNIFIED` and `SAA_R_UNIFIED` with KEY and ENC candidates on hardware.
4. Add a runtime sensor-slot strategy, then wire JOY's ADC and encoder candidates into the unified firmware.
5. Resolve the TPD/IQS/OLED I2C resource plan before wiring TPD into the unified firmware.
6. Add TB and TPD candidates after their pin and bus conflicts are resolved.
7. Keep snippet-based per-module builds only as diagnostics until the unified target is proven.
