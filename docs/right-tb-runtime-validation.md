# Right TB / left ENC validation

Branch: `codex/ananta-tb-runtime-fix`, based on released `3e6a6ea`.

- Right TB normal motion now reaches DYA's `mouse` runtime processor.
- Default AML target is Mouse (layer 3). Saved DYA settings take precedence;
  check its enable flag and target rather than resetting all settings.
- Fn (layer 1) and Scr (layer 4) replace the normal chain with XY-to-wheel
  mapping and DYA's `scroll` processor. Existing axis orientation is preserved.
- Ananta-local generated PMW source rejects MOT-clear and all-0xff samples,
  clears accumulated deltas, and limits diagnostic messages to once per second.
  The source hash is checked; a different upstream requires review.
- Shared west modules, hold-tap timing, BLE parameters and saved settings are
  not modified. No firmware has been flashed by this change.

Validation: host guard tests compile and execute the actual injected C;
layout check passes (58 keys, 6 layers). Right TB build and generated DTS/
source selection checked with just.sh. Left ENC and right TB builds both
passed (including the additional IQS variants selected by the build filter).

Hardware checks still needed: boot/vibration runaway, Fn scrolling, AML,
ordinary typing on each half, modifier+ball motion, and left UF2 application.
The guard is a mitigation, not proof that the reported runaway is resolved.
Typing stalls remain undiagnosed; do not claim the motion guard fixes them.
