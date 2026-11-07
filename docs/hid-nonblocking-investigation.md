# Ananta bounded send path (experimental)

The one-report fairness patch did not bound an individual ATT allocation:
pinned Zephyr's att.c uses K_FOREVER on the dedicated HOG workqueue.
The old producer path also waited 100ms per queue insertion attempt.

## Changes
- Preserve the pinned HOG GATT service; replace only its send queues.
- Notify from Zephyr system workqueue, where pinned ATT allocation and lower
  connection-buffer allocation explicitly use K_NO_WAIT.
- One attempt per report kind per invocation, keyboard first; retry after 8ms
  when no progress, 1ms when progress is made. No sleep, drain loop, recursive
  enqueue or queue-space wait in this path.
- Keep a failed notification pending rather than discarding it.
- Producers use a bounded spinlocked ring; no transport work while holding lock.
- Merge adjacent mouse movements only when profile and button state agree,
  using int64 accumulators; split into int16 chunks and subtract only on success.
- Preserve button transitions while capacity permits.
- At ring saturation preserve the oldest entries and replace the newest tail
  with the latest absolute state. This ensures final key/button release convergence,
  but intermediate taps/clicks can be lost under sustained overload. Mouse
  movement across a replaced button boundary can also be lost rather than
  reassigned to a different drag. Overflow counters are maintained internally.
  Finite storage is NOT a lossless guarantee for an indefinitely unavailable link.
- Tag frames with profile; compare to the actual referenced connection's peer
  profile before sending. Clear pending input when there is no active connection.
- PMW input_report failures retain rejected axes and int16 remainders.
  A pending-sync flag flushes accepted X even if rejected Y later cancels to zero.
  Existing delayed motion work retries without needing another motion IRQ.

## Validation
tests/renode_hog runs the actual replacement send implementation with real
Zephyr queues/scheduling on Renode nrf52840dk. Connection/notification are mocks,
including allocation failure. This does NOT emulate actual RF or PMW optics.
Assertions cover producer latency, notification context, delayed recovery,
100000/-100000 retained relative counts, final key release at saturation,
mouse press/release separation, saturated button release convergence,
bounded security retries, disconnect clearing and profile changes.
Observed virtual producer maximum: 0ms at the timer's millisecond resolution;
this is not a claim of zero CPU time or real-device latency.
Original comparison recorded 204ms key delay vs 2ms after fairness-only;
the new test reproduces a 150ms transport outage without producer sleep.

Host tests compile the generated PMW output block and cover failed X/Y enqueue,
partial XY acceptance, cancelled Y with pending sync, and >int16 remainders.
The existing driver flush/guard, scaling, layout and fairness regression tests remain.

## Reproduction
Use the manifest's pinned Zephyr (10ba6d0cb38bc3d258775d27982f707599320085),
SDK 0.16.9, nrf52840dk/nrf52840. Build tests/renode_hog as a standalone app;
set ZEPHYR_MODULES to only the workspace hal/cmsis and hal/nordic modules.
Load its zephyr.elf into Renode 1.16.1's nrf52840dk_nrf52840.repl, attach UART0
file backend, run for 4 virtual seconds, and require ALL_TESTS_PASS in UART output.
Host tests: set HOG_SOURCE and PMW_SOURCE to the manifest-pinned sources,
then python3 -m unittest discover -s tests.

## Limits
This bounds the application send path's waits; it cannot make a stalled link
deliver immediately or prevent unrelated system-workqueue work delaying dispatch.
PMW bus/capture mutexes and optical behavior are not proven bounded by these tests.
Frame capture intentionally pauses normal motion. Existing invalid-burst guards
can discard invalid sensor data. No real device has been flashed or tested.
Do not claim all reported stalls or cross-half timestamp ordering are resolved.
Do not publish the previous fairness-only draft as containing this implementation.
