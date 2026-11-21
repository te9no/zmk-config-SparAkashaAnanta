# PMW motion work queue comparison

This is an unverified comparison candidate, not a confirmed typing-latency fix.

## Scope

- Move PMW motion work from system workqueue to `saa_pmw_motion`, a preemptible
  priority-5 queue with a 2048-byte stack, started before motion IRQ enable.
- Keep initialization on its existing queue. Keep matrix, keymap, hold-tap,
  BLE connection parameters and stored Studio settings unchanged.
- Protect the entire motion transaction with the existing recursive device
  mutex, including IRQ re-enable. Do not re-enable IRQ during frame capture.
- Shared west sources are untouched; the keyboard-local generated driver is
  pinned by source hash and fails closed if patch anchors change.

## Limitations

This does not add timestamps to split notifications or reorder left/right
events. It does not remove the shared keyboard/mouse BLE HID send queue.
It does not yet add timing instrumentation or demonstrate real-device latency
improvement. Stack headroom and concurrent Studio diagnostics require testing.
The existing five-way geometry issue remains unresolved; this candidate does
not claim to correct it.

## Hardware comparison required

Preserve settings and pairing. With right TB / left ENC, compare the released
image against this candidate under the same connection conditions:

1. Both halves battery-powered, PC over BLE: type alternating A/I without motion.
2. Repeat while deliberately moving the ball, and while holding Shift.
3. Compare explicit USB HID on the right central (USB power alone is not proof
   that USB HID is selected); leave the left half wireless.
4. Verify scrolling, AML, CPI changes and Studio frame capture separately;
   do not stream sensor frames during the typing comparison.
5. Check `saa_pmw_motion` stack headroom and watchdog history after sustained use.

If typing improves but A/I inversion remains, investigate split transport
timing separately rather than declaring the issue resolved.

## Manual mouse-layer follow-up

The follow-up defaults disable right-TB AML and assign base-layer position 45
(the ordinary bottom-right Right Arrow key, not the five-way switch) to
`&lt MOUSE RIGHT`. A tap sends Right Arrow; a hold activates Mouse using the
existing 350 ms tap-preferred LT configuration. Layer 3 keeps that position
transparent. Mouse-layer Comma, Dot and Up send left, middle and right clicks;
the old J/K/L click positions are transparent again.

Existing Studio settings override both defaults. On an already configured
device, export settings first, disable mouse processor AML and save, then
assign position 45 on Def to LT layer 3 / Right Arrow, and Mouse positions
32/33/34 to left/middle/right click. Restore Mouse positions 19/20/21 to
transparent if using the old J/K/L clicks. Save. Preserve unrelated
bindings and parameters; do not use settings_reset. Firmware build success
alone does not demonstrate these saved settings were changed on the device.
