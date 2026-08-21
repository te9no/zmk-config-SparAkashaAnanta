# ZMK 0.4 migration

This branch establishes the ZMK 0.4 compatibility baseline before changing
the IQS9151 driver implementation.

- ZMK revision: `afe241df80b05c3f4e0cc95ada7584d24422a893`
- Board target: `xiao_ble//zmk`
- Existing `te9no/zmk-driver-iqs9151-rpc` integration is retained
- External PMW3610 and Cirque modules are removed because Zephyr 4.1 provides
  the `pixart,pmw3610` and `cirque,pinnacle` drivers
- PMW3610 and Cirque overlays use their Zephyr 4.1 devicetree properties
- NFCT GPIO configuration is moved from deprecated Kconfig to the UICR node
- USB logging and Studio UART snippets are limited to the split central

Keeping the IQS implementation unchanged makes this PR a reusable baseline
for separating general ZMK 0.4 compatibility fixes from the subsequent
upstream IQS9151 driver migration.
