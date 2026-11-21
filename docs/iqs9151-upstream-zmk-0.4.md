# IQS9151 upstream driver on ZMK 0.4

This change propagates the IQS9151 configuration proven on GeaconPolaris after
the general SAA ZMK 0.4 compatibility baseline was established separately.

- ZMK: `afe241df80b05c3f4e0cc95ada7584d24422a893`
- Driver: `ShiniNet/zmk-driver-iqs9151`
- Driver revision: `08a6fd19c5aa5ae7f11daf371b5a391cd8596783`
- Polaris verification: `839f0c090afcb3c5f6172ca12ea169ae36575466`

The fork-only `CONFIG_INPUT_IQS9151_FLIP_Y` option is removed. SAA keeps its
existing input-listener transforms, split routing, I2C bus, and pin mapping so
the hardware-specific orientation and wiring remain local to this repository.
