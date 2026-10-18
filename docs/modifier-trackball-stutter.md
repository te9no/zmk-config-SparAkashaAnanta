# Modifier + trackball stutter investigation (2026-10-18)

## Report

Ananta, left ENC / right TB: pointer briefly hesitates while a key is held,
more noticeable over BLE. Ordinary keys and Shift both reproduce according
to the user. Repetition during a hold was reported tentatively. Do not treat
this as only a hold-tap problem or assume keyboard autorepeat sends BLE reports.

## Code evidence, not a measured device cause

Inspected local candidate: `366757905e410f1b9cf8b09c5e90035d6171f2fd`.
The firmware currently installed on the affected device is not identified.

- Local ZMK `hog.c` uses one work queue for keyboard and mouse notifications.
  Enqueue operations can wait 100 ms when their message queue is full.
  Actual queue congestion has **not** been observed.
- Local PMW driver clears pending movement when the interval since the previous
  sample reaches the configured reporting interval (14 ms in the local build).
  `scripts/audit_pmw_report_gap.py SOURCE` compiles the actual relevant C block:
  +10 at 5 ms remains pending; +1 at 25 ms produces only +1, losing 10 counts.
  This demonstrates that code path, not that the device experiences that timing.
- The PMW driver submits input events with `K_NO_WAIT` without checking results.
  Input queue overflow has **not** been observed.
- Activity state changes are deduplicated in `activity.c`; ordinary presses do
  not unconditionally reprogram PMW performance while already active.

The local build profile used for inspection is `saa-xiao-pinmux`. Its generated
configuration is historical evidence, not proof of the currently installed image.
No latency fix has been flashed or published in this investigation.

## Required next evidence

1. Identify the connected right central and its installed firmware; preserve
   existing Studio settings, keymap and pairing.
2. Obtain bounded, aggregated timing/counter diagnostics for PMW work latency,
   input enqueue failures and BLE report enqueue waits/failures. Avoid per-event
   logs that could create the latency being measured.
3. Compare motion alone and motion with held Shift over BLE, then USB HID as a
   control. Keep transport selection explicit: USB power alone is not USB HID.
4. Correct the measured failure path, build and repeat the same comparisons.
   Verify both halves, normal motion, scrolling and AML before closing.

## Hardware evidence after diagnostics update

Both halves were subsequently identified and flashed; see
`studio-diagnostics.md` for serials and image hashes. User supplied a Studio
report matching right ID `0e1bc6b1940be806`, config `3667579-dirty`, ZMK
`e5c9b691`, build time `2026-10-18T08:26:56Z`.

At uptime 4m12s: PMW ready, product 0x3e, revision 0x01, init error 0,
CPI 800. Watchdog incident count 0 (freeze detection intentionally disabled).
Reported stack peaks: display 2600/3072, logging 648/768, sysworkq 2116/3072,
input 520/2048, HID-over-GATT work 176/2048 bytes. Stack headroom does not
measure execution latency or queue congestion. Key diagnostics showed one
suspect position but zero total presses; this is not verified chatter.

Right debug COM83 initially returned access denied, but subsequently opened.
A 20-second capture at 115200 baud with DTR/RTS asserted returned zero characters.
The user has not confirmed performing the reproduction during that window;
absence of logs proves neither normal operation nor absence of motion.
Generated config has deferred UART logging/autostart enabled, PMW level INFO,
and no runtime filtering. Left COM82 and right Studio COM84 enumerate.
Do not terminate another process to seize a port. Await the user's BLE
Shift+motion reproduction result with Studio disconnected. USB power does not
establish the HID endpoint. No post-update reproduction result is available.
COM45 (`VID_0ECB&PID_1F63`) remains unrelated and must not be reset or flashed.

## Port correction and logging comparison

The previous port interpretation was wrong: COM84 (MI_00) is the right CDC
console; COM83 (MI_03) is Studio RPC. Generated DTS lists the logging CDC before
Studio CDC. Reading COM84 returned actual motion logs, confirming the mapping.
Prior COM83 silence is not a logging failure.

The user confirmed stutter persists with the diagnostics firmware. COM84 showed
per-motion DEBUG output (`apply_config`, `scale_val`, HID movement/scroll) and
`65 messages dropped`, `8 messages dropped`, `40 messages dropped` notices.
These are **log** drops, not proven HID report drops. They establish excessive
logging, but not its causality for the reported cursor hesitation.

Comparison candidate explicitly selects `CONFIG_ZMK_LOG_LEVEL_INF=y` on the
right central, retaining CDC and all diagnostic RPCs. No BLE interval, motion
filter, scaling or key binding change is included. Build/flash and user A/B
confirmation are required before attributing any improvement to this change.

INFO selection alone left generated `CONFIG_ZMK_LOG_LEVEL=4`: ZMK's earlier
default overrides the normal log-choice default unless `ZMK_LOGGING_MINIMAL=y`.
Added that option; generated right-TB config now has level 3. TB and TB_IQS
builds succeeded. Right TB image SHA256
`f90a5e50e9fd57ead3be8f463f88ca21f6896379aea75d504a9fb21bff3ec7ce`
was flashed to serial `0E1BC6B1940BE806`. Previous diagnostic image backed up as
`ananta-right-before-quiet-log-20261018`. COM84 boot logs show PMW initialized,
saved mouse scale 1/1, scroll scale 1/60 and peripheral battery received.
Boot selected **USB HID**, despite BLE profile 2 also connecting. The user must
select BLE HID (or disconnect USB) for the wireless comparison. Improvement
is not yet confirmed. Final host-test rerun encountered a WSL service timeout;
firmware compilation and generated config verification succeeded separately.

## Thumb Space / Layer-tap comparison

User reports left thumb Space frequently requires repeated presses, confirms
it is a dual-role key, and requests parameter adjustment. Changed the shared
`&lt` behavior from balanced/200ms/175ms to tap-preferred/280ms/200ms; prior-idle
remains 150ms. All LT bindings are affected, not only Space. MT is unchanged.
This trades slower deliberate layer activation for fewer holds during typing;
it does not prove that lost presses or pointer latency are fixed.

Ten host tests passed. Right TB generated DTS confirms flavor and all three
timings. Right TB UF2 SHA256
`16036bcb37a48e803d7f5fd51a7d475f897df8f02172cf23df82372609abdf7b`
copied successfully to matched right serial `0E1BC6B1940BE806`. Previous image
backed up as `ananta-right-before-tap-preferred-20261018`. Saved bindings and
pairings were not reset. User validation of Space, intentional layer holds and
BLE Shift+motion remains pending.

User requested a longer LT threshold: raised only tapping-term from 280 to
350 ms; tap-preferred, quick-tap 200 ms, prior-idle 150 ms and MT unchanged.
Ten host tests passed and right-TB generated DTS confirms 350 ms (0x15e).
Right serial `0E1BC6B1940BE806` flashed with SHA256
`4b6a33e0294d6f13eeeb2710f04a1ba3fba8730024c8bec8acb06f80d6f42aa4`.
Prior image backed up as `ananta-right-before-lt350-20261018`.
Hardware usability and stutter outcome remain unconfirmed.

## Solstice comparison: motion burst (2026-10-18)

User reports no stutter on Solstice. Current local Solstice source uses
hardware SPI and motion burst on the right peripheral; Ananta uses GPIO
three-wire SPI on the right central. Installed Solstice firmware identity
has not been verified. The two manifests pin the same ZMK and PMW revisions.

Removed `disable-burst-read` from Ananta right TB only. Keep wiring, central
role, motion guards, scaling, logging and LT 350 ms unchanged. The GPIO SPI
controller supports multi-byte reads with CS held and SDIO turnaround.
This is a comparison candidate, not a proven stutter fix; it also selects
the driver's burst frame-capture path, which needs separate hardware testing.

Eleven host tests pass; right TB and TB_IQS builds succeed. Generated right
TB DTS has no disable-burst-read property. Right TB UF2 SHA256:
`0a1539cf615a7bc93c6278648f1bdedabe99a86baf7c19dd7608588124afd7cd`.
Not flashed: only left COM82 was enumerated, no right COM83/84 or removable
bootloader volume. BLE Shift+motion and ordinary key+motion tests pending.

Subsequent USB connection: verified right serial `0E1BC6B1940BE806`,
triggered COM84 at 1200 baud, and backed up CURRENT.UF2/INFO_UF2.TXT to
`.zmk-workspace/ananta-right-before-burst-20261018-182752` before flashing
the above hash. COM83/84 re-enumerated. CDC boot log reports product ID
0x3e, PMW3610 initialized, CPI 800, and peripheral battery received.
Endpoint is USB despite BLE profile 2 connected; a BLE endpoint test is
still required. No motion/stutter result claimed from initialization alone.

User subsequently tested the requested wireless key-hold + trackball case:
"めっちゃ改善した", followed by "ひっかかりません". Treat this tested
right-TB/left-ENC configuration as resolved for the reported stutter, without
claiming every variant or long-duration workload has been hardware tested.
User approved this configuration for the next release.
