# Scan startup follow-up (2026-11-09)

rc.3 feedback: direction changes on the five-way switch still intermittently fail;
some keys do not respond immediately after installation. Frequency might have
improved, but rc.3 is not a confirmed solution. Hardware is unavailable.

## Confirmed startup ordering defect

Pinned `physical_layouts.c` selects/enables a scan layout in APPLICATION init.
It calls PM resume before `kscan_config`. Composite resume enables its children;
charlieplex resume directly scans. Thus scanning can run before callbacks are
registered. Scanning also starts before `main()` restores persisted settings.
A held key / early scan event can therefore be processed using incomplete state.
This is a code-level lifecycle defect, not proof of the reported hardware cause.

The local patch now:

1. Initializes the default position map without enabling scanning.
2. Allows persisted layout selection while scans remain stopped.
3. Starts scanning after main's full settings load, before display startup.
4. Registers the callback before resuming the scanner.
5. Propagates configuration/start errors and makes the startup entry idempotent.

The existing post-start layout-change path still disables the old scanner and
starts the new one. PM runtime / ordinary PM configuration paths are retained.
Device-less layouts are supported. Source hash guards also cover `main.c`.

## Validation and limitations

Host tests compile the original and patched layout-selection functions with a
mock PM driver that synchronously reports input during resume. Original ordering
produces input before callback registration/settings restoration; the patched
ordering does not. Persisted layout selection, failed callback configuration,
retry, idempotence and later layout switching are checked. Total host regression
suite: 29 passed.

This test models the documented behavior visible in the pinned composite and
charlieplex driver source. It is not an electrical switch simulation, a full
firmware boot in Renode, or an actual NVS/RF test.

Left ENC/right TB firmware builds are checked separately. No hardware validation
or release publication is performed as part of this follow-up.

## Unresolved direction-switch symptom

The supplied report does not establish whether the missed press disappears in
raw scanning, after keymap/combo processing, or at host delivery. Both scanner
settling settings are zero; this is a testable electrical-timing hypothesis,
not proof of a defect. Normal direction changes have not reproduced a lockout
in the software queue tests. Do not claim the startup fix resolves this symptom
or change debounce/settling blindly to mask it.

Existing Studio scan statistics subscribe to processed position events; they
are not independent raw GPIO measurements. Further diagnosis needs either raw
scan-vs-delivery traces during a failure or an accurate circuit model. Do not
clear the user's persisted keymap/combos as a speculative workaround.

## Renode follow-up

The earlier host-only validation has now been supplemented with
[`renode_scan_startup`](../tests/renode_scan_startup/README.md). Both original and
patched production selector/main functions ran on nRF52840 Renode with actual
Zephyr scheduling, PM and settings handler dispatch. The baseline recorded 32
pre-restoration scans and one pre-registration scan, losing the startup-held
press in the modeled scanner. The fixed version recorded zero for both and
correctly delivered 26 transitions, including release, layout change and suspend/
resume. Both runs reached their expected `ALL_TESTS_PASS` assertions.

The scanner is mocked and settings use no flash backend. This verifies lifecycle
ordering, not the physical circuit, actual saved settings, RF delivery, full
firmware startup or the unresolved five-way symptom. Firmware source is unchanged
from `6faeece`; this follow-up adds tests and evidence only.
