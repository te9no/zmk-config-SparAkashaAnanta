# Five-way key state recovery (2026-11-08)

## Scope and evidence

Based on input-motion rc.2 (`f8e899de901ed94c14381ed5162a4efb9b0033c5`).
The report describes intermittent missing horizontal inputs after vertical inputs,
and vice versa, more frequently on the peripheral half. No hardware is available
for this change. The reported root cause is **not yet confirmed**.

The pinned code has three concrete loss paths:

- `physical_layouts.c`: a full four-entry scan queue silently drops transitions.
  The scanner has already committed its debounced state, so a dropped release
  is not automatically emitted again.
- Split central: the received bitmap is updated before individual transitions
  enter its bounded event queue; a failed enqueue can leave downstream key state
  stale even after later identical snapshots arrive.
- Split peripheral: position enqueue can wait 100 ms and recursively discard an
  older snapshot. A notification failure discards the dequeued snapshot.

An isolated Renode reproduction with the original scan callback submitted five
transitions while its consumer was held. Four arrived; the physical key was
released but the consumer still considered it pressed. A later press/release
repaired it. This proves a loss path, not reproduction of the user's hardware.

## Changes

Normal scan/central events retain FIFO order. On overflow, retain the desired
state, drain the already queued prefix, then reconcile missing releases before
missing presses. Later non-key traffic cannot indefinitely postpone repair.
Central state is indexed separately per peripheral, with bounds checks.

Peripheral position snapshots use nonblocking enqueue. The scheduled sender
attempts one snapshot per invocation on the system workqueue (the pinned ATT
allocator uses a nonblocking allocation in that context). A failed snapshot stays
at the head and is retried after 8 ms; successful progress continues after 1 ms.
When saturated, preserve the queued prefix and the latest complete state.
Subscription disable invalidates pending state; enable seeds the current state.
No key-position producer waits for transport buffers.

The Ananta-local generator checks exact input SHA256s and replaces only build
copies. It requires both expected source files. Shared west modules are not edited.
The physical matrix adapter is intentionally limited to Ananta's fixed 7 x 12
matrix; do not reuse it for arbitrary keyboards or changing physical layouts.

Under saturation, intermediate very short taps can still coalesce. This change
protects final pressed/released state, not lossless unlimited buffering. Sensor
and relay-event paths are unchanged and can still wait on their existing queues.
This does not claim all firmware waits have been eliminated.

## Validation

- Host regressions: 27 tests passed, including pinned-source interception,
  source hash guards, preserved unrelated services and existing input/motion tests.
- nRF52840 Renode + real Zephyr kernel: `tests/renode_key_state/main.c` includes
  the production queue and position sender, with a mocked notification endpoint.
  Normal FIFO, scan overflow release repair without another input, repeated
  direction changes, separate peripheral indices, non-key flood, invalid indices,
  failed notification retry, nonblocking producer, final release convergence,
  subscription disable/re-enable all passed (`ALL_TESTS_PASS`).
- Left ENC / right TB nRF52840 firmware builds passed.
- No physical switch, RF timing, host interaction or long-duration hardware test
  has been performed. Renode emits unsupported peripheral warnings; its test
  covers software scheduling and queues, not electrical characteristics.

Reproduce host tests with `HOG_SOURCE` pointing to the pinned, recovery-bridge
patched `zmk/app/src/hog.c`, and `PMW_SOURCE` pointing to the pinned driver:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -v
```

For the Zephyr test, build `tests/renode_key_state` for
`nrf52840dk/nrf52840`, supplying only the workspace CMSIS and Nordic HAL modules
in `ZEPHYR_MODULES`. Load its ELF using Renode 1.16.1's
`platforms/boards/nrf52840dk_nrf52840.repl`, attach the UART file backend, run three
simulated seconds, and require `ALL_TESTS_PASS` with no assertion failure.

## Hardware acceptance still needed

Install matching left/right builds. Test vertical then horizontal and horizontal
then vertical five-way sequences on both halves, first without trackball motion,
then during sustained trackball motion over wireless. Verify all releases, no
stuck directions, no missing ordinary taps, reconnect with a direction held then
released, and Studio edits. If the symptom persists, capture raw scanner changes
and downstream events; investigate electrical settling/debounce independently
instead of assuming this queue defect explains every instance.
