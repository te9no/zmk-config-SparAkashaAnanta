# Ananta ZMK 0.4 — 2026-10-25 (revision 2)

## Changes

- Correct the right five-way switch's electrical-to-logical direction mapping.
  Previously, physical Left / Up / Right / Down activated Up / Right / Down / Left.
  Center press and the left-hand cluster are unchanged.
- Keep logical key positions, Studio layout coordinates, and stored bindings unchanged.
  No settings reset is required. Assignments previously changed to compensate for
  the incorrect directions may need to be restored to their intended positions.
- Retain the October 25 OLED scheduling and trackball processing improvements.

## Verification and limitations

The user confirmed correct right five-way operation and DYA Studio editing/saving
after flashing the corrected right TB build (left ENC connected). This is an
observed successful test, not a confirmed root-cause fix for every Studio disconnect.
Long-duration stability and other hardware combinations remain unverified.

All 21 targets are included: 20 left/right module variants with/without IQS,
plus settings_reset. Build success does not imply hardware verification of every
variant. Choose the matching side/module UF2. Do not use settings_reset for a
routine update; power off before changing modules.

Each UF2 is accompanied by provenance JSON. SHA256SUMS covers all UF2 and JSON files.
