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
# 2026-10-16 follow-up candidate

User confirmed AML and ball scrolling work with rc.1, but reported unstable
pointer scaling and suspected Layer-tap latency. This follow-up is not yet
hardware validated or published.

- Layer-tap now explicitly uses balanced, 200 ms tapping term, 175 ms quick tap
  and 150 ms prior idle. This resolves holds on another key's completed tap and
  favors tap while typing rapidly. It is a tradeoff: Fn immediately after typing
  may intentionally resolve as tap. A dedicated momentary-layer key remains the
  option when immediate hold activation is required. Saved bindings are not reset.
- PMW activity performance uses normal-run 0x00, matching efog defaults and the
  Equinox comparison that received initial user confirmation. Ananta is untested.
- The pinned runtime scaler narrowed the multiplication into int16 before
  division (1000 * 100 / 100 did not remain 1000). Build-local substitution uses
  int64 intermediate arithmetic, saturates the int16 output instead of wrapping,
  and clears fractional carry when output clips. Ordinary fractional carry is
  preserved. The existing int16 remainder ABI still limits precision for divisors
  above 32767; this is not a new arbitrary-precision scaler API.
- Shared west checkouts, module revisions, scaling settings and direction are
  unchanged. Both patches verify pinned source hashes before generation.

Not addressed by this candidate: DYA's separate numerator/denominator RPC writes,
or intentional bypass when a processor's saved active-layer mask excludes the
current layers. Exported customer settings and matching DYA version are still
needed to diagnose those cases. Do not claim all scaling symptoms fixed.

