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

`config/west.yml` pins Zephyr to `te9no/zephyr` revision `af6fff80212a92f56c6ca9a3a339ab4957a85334`. That revision provides `zephyr,deferred-init`, `device_init()`, and the DTS validation support needed for non-base ZMK bindings.

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

Legacy per-module targets have been removed. `build.yaml` now treats `ModuleMux` as the only supported firmware path.

## Known Constraints

- `config/west.yml` pins the Zephyr deferred-init revision on `te9no/zephyr`.
- `build.yaml` is unified-firmware-only. `ModuleMux` is the supported path for normal SAA firmware.
- `devices` are currently wired for `KEY`, `ENC`, `JOY`, `TB`, and `TPD` through the `ModuleMux` snippet. IQS is included with the unified firmware and remains independent from the mutually-exclusive base module profiles.
- First boot initializes the `KEY` candidate by default. A non-key module can be selected through the behavior and then restored on the next boot, but the hardware behavior of that first boot still needs validation.
- `JOY` now builds through `ModuleMux`, but analog event routing and encoder normalization still need hardware validation against the original `JOY` snippet behavior.
- `TB` now builds through `ModuleMux`, but SPI2 deferred bus wake-up and PMW3610 behavior still need hardware validation against the original `TB` snippet behavior.
- `TPD` now builds through `ModuleMux` using `gpio-i2c`, but Cirque behavior and split input routing still need hardware validation against the original `TPD` snippet behavior.

## 単一ファームウェア化ロードマップ

### Phase 0: 現状整理

- 済: IDピン前提の自動判別実装を撤回。
- 済: ユーザー選択式の `saa_mod` behavior を追加。
- 済: 選択したモジュールプロファイルを Zephyr settings に保存し、起動時に復元。
- 済: IQS を排他モジュールではなく、各ベースプロファイルと共存できる常設オプションとして扱う。
- 済: `KEY=GPIO`、`ENC=Encoder`、`JOY=ADC+Encoder`、`TB=SPI`、`TPD=I2C` の対応表を汎用 input module API として公開。
- 済: settings 読み込み後の apply point として `zmk,input-module-mux` を追加。

### Phase 1: Deferred Init 基盤

- 済: ローカル Zephyr にグローバルな `zephyr,deferred-init` devicetree property 対応を追加。
- 済: settings 読み込み後に deferred device を初期化するための `device_init(dev)` を追加。
- 済: Zephyr deferred-init branch を `te9no/zephyr` に公開。
- 済: `config/west.yml` で Zephyr revision `af6fff80212a92f56c6ca9a3a339ab4957a85334` を固定。
- 済: ローカルビルドが Zephyr build `af6fff80212a` を使うことを確認。
- 済: GitHub Actions のログでも Zephyr build `af6fff80212a` が使われていることを確認。

### Phase 2: SAA Module Mux の実体化

- 済: mux binding に候補 device phandle を追加。
- 済: 選択プロファイルに応じて候補 device を `device_init()` する mux 入口を実装。
- 済: `KEY` 候補を deferred direct GPIO kscan として定義。
- 済: `ENC` 候補を deferred left/right encoder device として定義。
- 済: `JOY` 候補を deferred analog input + deferred encoder device として定義。
- 済: `TB` 候補を deferred `spi2` + deferred PMW3610 trackball として定義。
- 済: `TPD` 候補を deferred `gpio-i2c` + deferred Cirque Pinnacle touchpad として定義。
- 済: 排他的な各候補経路に `zephyr,deferred-init` を付与。

### Phase 3: Snippet 再設計

- 済: `snippets/ModuleMux` を通常のモジュール経路に変更。
- 済: `KEY`、`ENC`、`JOY`、`TB`、`TPD` の定義を unified `ModuleMux` の候補グラフへ統合。
- 済: 旧排他 snippet である `snippets/KEY`、`snippets/ENC`、`snippets/JOY`、`snippets/TB`、`snippets/TPD` を削除。
- 済: `build.diagnostics.yaml` と旧 per-module diagnostic target を削除。
- 済: `IQS` は通常の左右 unified build に含める optional overlay として維持。
- 残: 実機検証後、IQS を snippet として残すか、SAA base / module-mux 定義へ統合するかを決める。

### Phase 4: 入力経路の競合解消

- 済: 通常起動では排他的な候補 device を初期化しない構成にした。
- 済: ZMK の通常 application init より前に保存済み profile を読み込み、選択された profile の device path だけを初期化。
- 済: `TB` の `spi2`、`TPD` の `tpd_i2c` など、必要な bus device も deferred 化。
- 済: `ENC` と `JOY` は `zmk,input-module-sensor-proxy` 経由にして、ZMK 側の sensor slot を安定化。
- 済: `KEY` の direct GPIO kscan 候補も deferred 化。
- 残: 未選択候補の pinctrl state が共有ピンに副作用を出さないか実機で確認。
- 残: input-listener / input-split / kscan が未初期化候補を runtime で触らないか確認。
- 残: deferred `gpio-i2c` bus を使った TPD split route を確認。
- 残: 保存済み profile がない初回起動で、`KEY` fallback が正しく動くか確認。

### Phase 5: 単一ファーム build.yaml

- 済: `build.yaml` を unified firmware path に変更。
- 済: 通常 build target から `TB`、`ENC`、`JOY`、`TPD`、`KEY` snippet 指定を削除。
- 済: 現在の target は `SAA_L_UNIFIED`、`SAA_R_UNIFIED`、`settings_reset`。
- 済: GitHub Actions build は reusable な `te9no/zmk-workspace` firmware workflow を使用。
- 済: ローカルの clean `./just.sh build all` で `SAA_L_UNIFIED`、`SAA_R_UNIFIED`、`settings_reset` が成功。
- 済: 最終 artifact 名は `SAA_L_UNIFIED` / `SAA_R_UNIFIED` のまま維持する。
- 残: cleanup 後の daily GitHub Actions build health を確認。

### Phase 6: Runtime UX

- 済: `BT` レイヤーに module profile 選択キーを追加。
- 済: profile 変更時に「次回起動から有効」と分かるログを出す。
- 方針: OLED への profile 表示は不要。表示系の変更は行わない。
- 残: 必要なら Studio RPC から現在 profile を読めるようにする。
- 残: profile 変更後の reboot 導線を behavior またはドキュメントとして用意。
- 残: settings reset 後や片側だけ変更した場合に、central / peripheral の profile 設定が分岐し得ることを明示。

### Phase 7: 実機検証

- 済: unified 左右ファームウェアと `settings_reset` の build 検証は通過。
- 残: `KEY` profile で direct key が動くことを確認。
- 残: `ENC` profile で encoder A/B が動くことを確認。
- 残: `JOY` profile で ADC + encoder A/B が動くことを確認。
- 残: `TB` profile で SPI/PMW3610 が動くことを確認。
- 残: `TPD` profile で I2C/Pinnacle が動くことを確認。
- 残: IQS が全 base profile と共存することを確認。
- 残: 左右 split で profile 選択、永続化、起動時復元が期待通り動くことを確認。
- 残: cold boot、warm reboot、settings reset の動作を確認。

### Phase 8: 汎用モジュール化

- 済: 再利用部分を SAA 専用名ではなく `zmk,input-module-*` 系の汎用名で実装。
- 済: SAA 固有の profile ID は `dt-bindings/saa/module_select.h` に隔離。
- 残: `src`、`dts/bindings`、`include/zmk` の input-module 関連を standalone な ZMK module として切り出す。
- 残: 各キーボード固有の profile ID、default profile、`settings-key` は各 config 側に残す。
- 残: GeaconPolaris や将来の modular input keyboard でも同じ module を再利用できるようにする。
- 残: Zephyr deferred-init patch を fork 前提で維持するか、upstream 提案するか判断。

## 現在の主なリスク

- 固定している Zephyr revision が利用できなくなる、または CI が期待する branch と乖離する。
- 既存の ZMK input-listener / input-split / kscan が、選択済み deferred path の初期化前に device を触る可能性がある。
- 未使用候補を deferred にしていても、共有ピンに対する devicetree / pinctrl の副作用が残る可能性がある。
- 外部ドライバが deferred init 前提で十分に安全に書かれていない可能性がある。
- 単一ファーム化により RAM / Flash 使用量が増え、将来の module 追加余地を圧迫する可能性がある。
- profile settings を片側だけ reset / 変更した場合、左右 split の profile が分岐する可能性がある。

## 次にやること

1. 実機検証を `KEY`、`ENC`、`JOY`、`TB`、`TPD` の順で進める。
2. Profile 変更後の reboot 導線を behavior またはドキュメントとして整理する。
3. SAA の実機検証後、汎用 `zmk,input-module-*` 部分を standalone module として切り出す。
