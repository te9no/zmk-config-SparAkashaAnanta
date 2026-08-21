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
- The CDC bootloader trigger node is likewise limited to the split central
- Nice! OLED uses its ZMK 0.4 battery-only screen on both split builds,
  avoiding legacy LVGL 8 animation assets while preserving the 90-degree
  peripheral battery display; the pinned revision also guards central-only
  APIs so the screen links on a split peripheral
- Removed legacy Cirque idle-sleeper and Studio RPC Kconfig options that are
  not provided by the Zephyr 4.1 input driver
- The animation driver is pinned to its ZMK 0.4 migration revision, which uses
  the current endpoint selection API on the split central
- Shared modules are pinned to the revisions already verified by Polaris;
  SAA-only animation, power, physical-layout, and IQS fork modules are pinned
  to their resolved commits for reproducible builds

Keeping the IQS implementation unchanged makes this PR a reusable baseline
for separating general ZMK 0.4 compatibility fixes from the subsequent
upstream IQS9151 driver migration.
