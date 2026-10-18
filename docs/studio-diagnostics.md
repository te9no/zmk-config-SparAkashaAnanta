# Ananta Studio diagnostics

Added 2026-10-18 on `codex/ananta-tb-runtime-fix`.

Right central exposes device info, watchdog crash history, key-scan diagnostics
and devtool stack high-water usage. Existing PMW diagnostics remain enabled.
Left peripheral records watchdog/key-scan data and answers split queries; its
key-scan **local Studio RPC is disabled**, because peripheral builds do not
generate the central Studio protocol headers. Both halves use a 256-byte relay
payload ceiling. Update both halves for peripheral diagnostic queries.

Modules are pinned in `config/west.yml`. No event tap, key injection or continuous
log capture is enabled. Watchdog fatal detection is enabled, but periodic freeze
detection is intentionally disabled during the radio-latency investigation.
Thus crash history is available; automatic freeze detection is not promised.
Stack measurement adds bookkeeping and startup stack painting.

Validation: 9 host tests passed. Local Docker builds succeeded for
`SAA_L_MODULE_ENC`, `SAA_L_MODULE_ENC_IQS`, `SAA_R_MODULE_TB` and
`SAA_R_MODULE_TB_IQS`. Generated right-TB `.config` confirms all four diagnostic
features and their RPCs plus stack usage and split relay. Build root:
`.zmk-workspace/profiles/saa-xiao-pinmux/build-diagnostics`.

Flashed 2026-10-18 after matching bootloader USB serials:

- Right TB: `0E1BC6B1940BE806`, SHA256
  `e2932241be8205d48051f93f2534dd60b806baf4dafa895eaaac1f49290d3bef`.
- Left ENC: `2A2C91BF2449BCE2`, SHA256
  `7c7a71c838a68d9fc7cfc1164ea7b0ded1dfcdf3c69f978347d764a402294dbe`.

Both UF2 copies completed and devices returned to application USB enumeration.
Left CDC emitted the Zephyr boot banner. Right CDC opened but returned no logs;
this is not proof that RPC or split connectivity works. Previous CURRENT.UF2 and
INFO_UF2.TXT were backed up in the workspace's `.zmk-workspace/backups` under
`ananta-{left,right}-before-diagnostics-20261018`.

Not pushed or released. Studio recognition, peripheral query replies, stack
headroom during use and the reported pointer stutter remain hardware verification
items. These diagnostics are not themselves a stutter fix.
