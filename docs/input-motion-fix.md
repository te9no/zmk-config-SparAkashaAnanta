# Ananta input and motion correction

This branch retains the pinned wireless recovery integration and addresses two
additional paths found during the input-order and small-motion investigation.

- The left OLED queue now uses priority 12, matching the right side. Radio host
  TX/RX threads (7/8) can run before display rendering.
- The PMW motion queue uses priority 9 so it also yields to radio host TX/RX.
- A sample with MOT clear contributes zero new motion and preserves previously
  measured pending movement. An all-ones bus response still clears it.
- A per-device delayed work item flushes pending movement at the report deadline.
  The motion IRQ is re-enabled so fresh movement can still be sampled during
  this bounded wait. No periodic polling is scheduled when movement is empty.
- Capture owns the IRQ while active. Read errors discard pending movement and
  terminate the delayed retry. Private generated headers keep the shared driver
  checkout intact.

Validation uses the generated production report function and work callback
compiled with a mocked sensor, clock and scheduler. It covers signed movement,
stale no-motion deltas, stop-before-deadline, additional motion at the deadline,
invalid bus responses, read errors, capture ownership and disabled rate limiting.
The existing layout, hold-tap, settings and arithmetic tests also run.

Hardware confirmation is pending. Left-to-right key ordering is still subject
to wireless delivery latency because the pinned split protocol sends key states
without original event timestamps. These scheduling changes remove avoidable
contention but do not guarantee chronological ordering across halves.

For acceptance, install matching left ENC and right TB builds. Test rapid
T-A-I sequences, continuous typing during ball motion, small isolated ball
movements, OLED rendering, Studio frame capture and idle/wake. Compare host
pointer acceleration settings when assessing speed-dependent pointer gain.
