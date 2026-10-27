# Ananta Bluetooth recovery — experimental prerelease

Based on the stable `zmk-0.4-20261025-2` release. This trial retains its OLED
scheduling, right five-way mapping, manual mouse layer and settings fixes.

## Changes

- Add pinned Bluetooth recovery module `7e76381d90856a42e739cebf66752afebbe78748`.
- Preserve cormoran ZMK `e5c9b6915b56801193e359dd9bad4a167ce0d1b8`;
  apply an explicit, version-checked bridge in an isolated build workspace.
- Right central: retry failed split scan starts. Left peripheral: retry failed
  advertising starts. Delays: 1/2/4/8/16 seconds, maximum five retries.
- Suspend recovery during sleep; allow a five-second boot/wake grace.
- Reject foreign connections as split peers; guard invalid peripheral indices.
- No bond erasure, periodic reboot, forced disconnect or stack restart.

## Download

All 20 module combinations are included (left/right, TB/JOY/ENC/TPD/KEY,
with/without IQS), plus `settings_reset.uf2`. Install matching variants on
both halves. This trial does not require a settings reset.
The reset utility deliberately does not enable Bluetooth recovery.

Each UF2 has a JSON provenance record; SHA256SUMS covers all UF2 and JSON files.
Use the dedicated Bluetooth recovery build artifacts, not old files in the
repository's firmware folder.

## Validation and limits

Build/provenance checks must pass for all 21 targets before publishing.
Native recovery policy and patched owner-function mock tests cover retry
limits, duplicate failures, foreign/PC connections, multiple peers and sleep.
The ordinary firmware publication workflow is skipped on the trial branch.

**This firmware has not been tested on Ananta hardware.** Long-duration idle,
BLE failure recovery, power consumption, sleep/wake and DYA Studio operation
remain unverified. A connected-but-unresponsive link or a stopped MCU is not
generally detected or repaired by this module. This prerelease does not claim
to resolve every freeze or the intermittent Studio disconnect report.

For rollback, install the prior stable release's matching variants.
This prerelease is not marked Latest and does not merge the trial into a
stable branch.

## Reproduce

Create a separate west workspace, copy `config/west.yml` to its
`manifest/west.yml`, then run `west init -l manifest` and
`west update --narrow`. From that workspace run:

```sh
python3 /path/to/config-repo/scripts/build-bt-recovery.py SAA_R_MODULE_TB
python3 /path/to/config-repo/scripts/build-bt-recovery.py SAA_L_MODULE_ENC
```

The script requires a clean config checkout and the pinned module revision.
The bridge installer rejects unsupported ZMK revisions. Source patching is
explicit and limited to that isolated ZMK checkout.
