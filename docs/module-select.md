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
- Optionally use `zmk,input-module-sensor-proxy` when several profiles need to share one ZMK sensor slot.

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
- Loads the selected profile early at `CONFIG_ZMK_INPUT_MODULE_SETTINGS_INIT_PRIORITY`.
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

## What This Still Does Not Do

This does not dynamically switch Devicetree nodes from `disabled` to `okay`. ZMK input devices are still described at compile time, but selected candidate devices can now be deferred and initialized after settings are loaded.

The saved profile is intended as runtime state for future filtering, UI display, diagnostics, or driver gating where the hardware definition allows it. The mux loads its settings subtree at `CONFIG_ZMK_INPUT_MODULE_SETTINGS_INIT_PRIORITY` before ZMK's normal `CONFIG_APPLICATION_INIT_PRIORITY` app initializers, then calls `device_init()` for the selected candidate devices. This can make deferred `okay` devices available before ZMK keymap, sensor, and physical layout setup, but it still cannot rewrite Devicetree or enable nodes that were not compiled into the firmware.

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

## Sensor Proxy

`zmk,input-module-sensor-proxy` is used by `ModuleMux` so ZMK's static `sensor-bindings` can remain stable while different module profiles use different physical encoder definitions.

`ENC` and `JOY` both use the encoder A/B pins, but their existing snippets use different `steps` and `triggers-per-rotation` values. The proxy keeps the keymap at two sensor slots and delegates to the selected profile's deferred encoder candidate:

- `ENC`: `left_encoder_enc` or `right_encoder_enc`, `steps = <24>`, normalized to 10 triggers per rotation.
- `JOY`: `left_encoder_joy` or `right_encoder_joy`, `steps = <3>`, normalized to 1 trigger per rotation.

If the active profile has no sensor route, such as `KEY`, the proxy accepts ZMK's trigger registration without touching the raw encoder pins.

## ModuleMux Snippet

`snippets/ModuleMux` is the first unified-module snippet. The normal `build.yaml` now builds this firmware path instead of per-module firmware variants.

Current candidate coverage:

| Profile | Candidate device | Deferred | Notes |
| ------- | ---------------- | -------- | ----- |
| `KEY` | `kscan2` direct GPIO kscan | yes | Default profile for first boot. Shares pins with encoder/TB paths, so runtime behavior must be checked carefully. |
| `ENC` | `left_encoder_enc` or `right_encoder_enc` | yes | Routed through `zmk,input-module-sensor-proxy`. |
| `JOY` | `anin0` plus `left_encoder_joy` or `right_encoder_joy` | yes | ADC is handled by `dya,analog-input`; encoder route is normalized through the sensor proxy. |
| `TB` | `spi2` plus `trackball` | yes | SPI2 and PMW3610 are both deferred; the mux initializes the bus before the trackball device. |
| `TPD` | `tpd_i2c` plus `glidepoint` | yes | Uses a deferred `gpio-i2c` bus on the TPD SDA/SCL pins because XIAO nRF52840 has no `i2c2`. |

The base SAA node keeps `default-profile = <SAA_MODULE_UNSPECIFIED>`, but `snippets/ModuleMux/ModuleMux.overlay` overrides it to `SAA_MODULE_KEY`. Without a default key profile, a freshly flashed board could have no initialized module input path and no practical way to select the saved profile from the keymap.

Normal build targets:

- `SAA_L_UNIFIED`: `Central ModuleMux IQS`
- `SAA_R_UNIFIED`: `Peripheral ModuleMux IQS`

Legacy per-module targets are kept in `build.diagnostics.yaml` for comparison and debugging.

## Known Constraints

- `config/west.yml` expects `feature/deferred-device-init` to exist on `te9no/zephyr`.
- `build.yaml` is unified-firmware-first. Use `build.diagnostics.yaml` for legacy per-module diagnostics.
- `devices` are currently wired for `KEY`, `ENC`, `JOY`, `TB`, and `TPD` through the `ModuleMux` snippet. IQS is included with the unified firmware and remains independent from the mutually-exclusive base module profiles.
- First boot initializes the `KEY` candidate by default. A non-key module can be selected through the behavior and then restored on the next boot, but the hardware behavior of that first boot still needs validation.
- `JOY` now builds through `ModuleMux`, but analog event routing and encoder normalization still need hardware validation against the original `JOY` snippet behavior.
- `TB` now builds through `ModuleMux`, but SPI2 deferred bus wake-up and PMW3610 behavior still need hardware validation against the original `TB` snippet behavior.
- `TPD` now builds through `ModuleMux` using `gpio-i2c`, but Cirque behavior and split input routing still need hardware validation against the original `TPD` snippet behavior.
- The legacy `snippets/TPD/TPD.overlay` still references `&i2c2`, which is not provided by the XIAO nRF52840 DTS. Use `ModuleMux` for the unified TPD path.

## Roadmap to One Firmware

1. Publish and maintain the Zephyr deferred-init branch pinned by `west.yml`.
2. Validate `SAA_L_UNIFIED` and `SAA_R_UNIFIED` with KEY, ENC, JOY, TB, and TPD candidates on hardware.
3. Decide whether the legacy per-module TPD snippet should be migrated to `gpio-i2c` or kept only as historical diagnostics.
4. Extract `zmk,input-module-*` and proxy primitives into a standalone reusable ZMK module.
5. Move module-specific input routing into reusable proxy primitives where the current ZMK static listener model is too rigid.
6. Keep snippet-based per-module builds only as diagnostics until the unified target is proven.
