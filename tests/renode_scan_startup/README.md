# Startup lifecycle on Renode

Uses the revision-checked **production layout selector and main function**, before
and after the startup patch, with real Zephyr initialization, device PM, settings
handler dispatch and system workqueue scheduling. The test scanner is a mock
kscan device whose PM resume synchronously samples state, as the pinned
charlieplex driver's PM resume does. Its held-state model is deliberately simple;
it does not emulate charlieplex GPIO, analog settling or debounce.

The settings subsystem is real, with SETTINGS_NONE and a static commit handler.
The handler selects the saved-layout surrogate and sleeps for 30 ms to permit
system workqueue execution before declaring restoration complete. No NVS flash
image or actual persisted user keymap is loaded.

From an isolated Zephyr/west environment (SDK 0.16.9, Zephyr
10ba6d0cb38bc3d258775d27982f707599320085), for FIXED=0 and FIXED=1:

```sh
west build -b nrf52840dk/nrf52840 -s /path/to/tests/renode_scan_startup \
  -d /tmp/scan-$FIXED -- -DFIXED=$FIXED \
  -DZMK_APP_SRC=/path/to/pinned/zmk/app/src \
  -DZEPHYR_MODULES="/path/to/modules/hal/cmsis;/path/to/modules/hal/nordic"
```

Use Renode 1.16.1 and the following script (absolute paths):

```text
mach create
machine LoadPlatformDescription @platforms/boards/nrf52840dk_nrf52840.repl
sysbus LoadELF @/tmp/scan-1/zephyr/zephyr.elf
sysbus.uart0 CreateFileBackend @/tmp/scan-1-uart.log true
emulation RunFor "2"
quit
```

Require UART `ALL_TESTS_PASS`, not merely a successful emulator exit. Baseline
passing means the expected defect was detected, not that baseline is correct.

Verified 2026-11-09:

```text
BASELINE: early=32 missing_callback=1 held_press_lost=YES
ALL_TESTS_PASS
FIXED: early=0 missing_callback=0 delivered=26; settings/held/release/layout-switch/suspend-resume PASS
ALL_TESTS_PASS
```

The fixed test covers startup-held press, release, 20 subsequent transitions,
post-start layout switch, idempotent startup and device suspend/resume. It verifies
that scanning stops while the mock device is suspended. This is not a full
Ananta firmware/RF/host simulation, a deep-sleep power-cycle test or reproduction
of the customer's five-way direction failure. Renode's board model emits some
unsupported-peripheral warnings; no assertion or CPU fault occurred in these runs.
