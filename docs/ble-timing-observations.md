# BLE timing comparison (2026-10-25)

Device: Ananta right TB, USB serial 68447FA3F4298CB1, CDC COM136.
The user confirmed AML disabled and reproduced stutter during the capture.
The log confirmed `Endpoint changed: BLE:0`.

First diagnostic: cumulative maxima during the reproduction were keyboard
enqueue 30 us, mouse enqueue 396 us, keyboard notify API 915 us, mouse notify
API 885 us. All four instrumented error counters remained zero. This does not
show the hypothesized 100 ms queue-full wait in this capture. It does not measure
queue residence, on-air timing, PC delivery, or sensor/input processing latency.

Second diagnostic adds intervals at enqueue entry and notification API entry.
Slots 0/1 are keyboard/mouse enqueue; 2/3 are keyboard/mouse notification.
`ge20` and `ge50` count intervals at least 20 and 50 ms, respectively, capped
at 500 ms for classification. Longer intervals increment `pauses` instead.
These are cumulative observations, not automatic stall classifications: stops
in user motion also create gaps. Compare continuous-motion windows, first
without keys, then with keys, and compare increments across the windows.

The notification call returns before confirmed host delivery. Do not conclude
that radio or host latency is absent from short API durations. No report values,
transport queue limits, or BLE connection parameters are changed by the timing
instrumentation. Logs are emitted only from the sender, at most once per 5 s.
No sensor frame streaming should run during the test.

The user also confirmed reproduction in the second capture. Mouse enqueue and
notify counts both reached 906, with no errors. Enqueue max was 61 us; mouse
notify max was 762 us. Both had a maximum interval of 306 ms (excluding gaps
over 500 ms). This localizes at least some spacing upstream of the HOG sender,
but does not identify the cause or rule out additional radio/host delays.

Next diagnostic counts motion-worker calls, read failures, reported samples,
input_report failures, rejected bursts and nonzero accumulated deltas purged by
the existing interval rule. It measures worker entry intervals, SPI-read elapsed
time, mutex wait and total work time. All counters are cumulative and worker-only;
two summary lines at most every five seconds. Work/read elapsed time includes
preemption. Worker entry gaps still include actual pauses in hand motion. This
diagnostic deliberately preserves the purge behavior and nonblocking input
reports to distinguish observation from a speculative fix.

Third capture (device uptime 1:19–2:17) observed 43 nonzero pending-motion
purges by 2:02.725, 2908 worker calls and 1472 reported samples. Sensor read
errors, input queue errors and rejected bursts remained zero. Maximum observed
SPI-read elapsed time was 3417 us, lock wait 396 us and work time 4119 us.
The 450 ms maximum worker gap includes pauses in hand motion. Purges also
occurred without additional keyboard reports, so this is not proof of the
entire key/mouse stutter cause.

Comparison fix: preserve pending motion across sample gaps, leaving the 14 ms
effective report interval, direction, scaling, invalid-burst guards and input
queue semantics unchanged. The counter is now named `retained_gaps`, not
`purges`. Native C regression verifies +10 followed by +1 after a gap reports
+11 rather than +1, negative Y movement, multiple pending samples and interval
zero. All 22 tests and right TB/TB+IQS builds passed. The right TB UF2 SHA256 is
`b2c6d64eb04540030aa3243faff783151e5f60d66651d790b9f5c9ead94e1f88`.
Subjective stutter improvement still requires the user's comparison; this is
not a verified resolution or a published release. This fix does not add a
timer to flush a final pending sample after all motion interrupts cease.

User comparison: **no perceived improvement** after the pending-motion fix.
Do not describe that fix as a stutter resolution. The next diagnostic adds GATT
notification completion latency and negotiated connection interval/latency.
Completion is the stack's notification completion, not host application display
time. A 32-bit cycle timestamp is passed by value via user_data (not a pointer
to stack storage). Completion callbacks only update atomic counters; summaries
remain rate limited to five seconds on the HOG queue. No BLE parameters change.
Saved generated Solstice central configuration requested minimum interval 6,
Ananta 12; both maximum 12 and peripheral latency 30. These are preferences,
not evidence of the actual negotiated connection timing.
The pinned Zephyr GATT API invokes completion callbacks from sysworkq: these
measurements include callback scheduling delay, not just transmission latency.
This diagnostic build's right TB hash is
`1b8e67e176802182e9d5d13928b0b1357895742eea0257cad7cd917f5aba8c61`.

Completion capture: negotiated interval 12 (15 ms), peripheral latency 30,
timeout 400 (4 s). Mouse completion maximum 14007 us; keyboard 20507 us.
PMW total work maximum 162933 us, versus SPI elapsed 2014 us and mutex wait
30 us. The long work maximum already occurred before the main typing window;
it is not proof of a key-triggered display stall. However, both the dedicated
motion queue and display queue have priority 5 with time slice size zero, and
the OLED redraws on key presses. Comparison changes only motion priority 5 to
4; input priority 0 and cooperative sysworkq stay higher priority. OLED remains
enabled. This tests scheduling contention without changing BLE or keymaps.

User result for priority 4: unchanged stutter. It is not a fix. Next comparison
uses the local-only `SAA_R_TB_DIAG_NO_OLED` target. `CONFIG_ZMK_DISPLAY=n` removes
the ZMK display update thread and custom OLED status-screen source (confirmed
absent from compile_commands.json). DISPLAY and LVGL libraries remain enabled
to satisfy the existing shield configuration. Left firmware is unchanged.
The normal right-TB artifact remains available separately. Do not publish this
diagnostic target. All 22 host tests passed. UF2 SHA256:
`83b2857a5331acf48406e6ac366ee28a0a98b9ea28f48e285a4dced3b5229bad`.

User result for no-OLED comparison: improved, then explicitly "stutter gone".
This implicates the display-enabled workload, not yet a specific drawing call.
Next comparison restores the normal OLED target and sets the central display
thread to priority 12, below motion 4, HOG 5, Bluetooth TX 7 / RX 8. Keymap and
BLE connection settings remain unchanged. Display animation remains enabled.
The normal-target build confirms `CONFIG_ZMK_DISPLAY=y` and display priority
12. All 22 tests and right TB/TB+IQS builds passed. Right TB UF2 SHA256:
`65b9b4ec59ed968bc533f4e848788159d7453d6c9f3a158547efbc3311c37d98`.

2026-10-25 JST user verification: "カクつきなし・OLEDも表示" for this
display-priority-12 firmware. OLED-off removed the symptom; restoring OLED
with lower display priority also removed it. The comparison supports display
scheduling contention as the cause of the observed typing/motion stutter.
Sensor-priority-only and pending-motion preservation alone did not resolve it.
Verified scope: connected Ananta right TB / left ENC, BLE typing plus trackball,
OLED visible. Other module combinations and long-duration behavior remain
unverified. No further flash, merge, push or release was performed on receipt
of this confirmation. Diagnostic instrumentation still remains in this local
branch; a cleaned production build must be separately tested before release.
