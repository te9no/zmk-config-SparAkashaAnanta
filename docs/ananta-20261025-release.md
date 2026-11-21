# Ananta ZMK 0.4 — 2026-10-25

## Changes

- Lower OLED rendering priority below input and Bluetooth processing to address
  cursor stutter while typing and moving the trackball over BLE.
- Move PMW3610 motion processing onto a dedicated, mutex-protected work queue.
- Preserve accumulated trackball movement across delayed samples.
- Remove temporary timing instrumentation and the OLED-off diagnostic target.
- Default keymap: hold the bottom-right Right Arrow key for the Mouse layer,
  tap for Right Arrow. On Mouse, comma / period / Up are left / middle / right
  mouse buttons. Automatic mouse layer activation is disabled by default.

Existing DYA Studio settings and keymaps are preserved and can override firmware
defaults. No settings reset is required for the scheduling fix.

## Verification and limitations

The user verified that the OLED-enabled scheduling comparison removed stutter
with **right TB + left ENC**, BLE typing and simultaneous trackball movement.
That comparison included diagnostic counters, which are removed from this
release. The cleaned build and other module combinations have not yet received
the same hardware verification. Long-duration testing remains unverified.

All 21 build targets are included: 20 left/right module variants (with/without
IQS) and settings_reset. Choose the UF2 matching your side and hardware; do not
flash settings_reset for a routine update. Power off before changing modules.

Each UF2 has a provenance JSON; SHA256SUMS covers every UF2 and JSON. Build
success for a variant is not a claim of hardware verification for that variant.
