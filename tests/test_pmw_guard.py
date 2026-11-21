from pathlib import Path
import importlib.util
import unittest
from unittest.mock import patch as mock_patch
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("guard", ROOT / "scripts/patch_pmw_guard.py")
guard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guard)


class BatteryAndGuard(unittest.TestCase):
    def test_unreviewed_source_rejected(self):
        with self.assertRaises(ValueError):
            guard.patch("unexpected upstream source")

    @unittest.skipUnless(shutil.which("cc"), "host C compiler required")
    def test_compiled_motion_guard(self):
        # Compile the exact injected C against small driver/logging stubs.
        with mock_patch.object(guard.hashlib, "sha256") as sha:
            sha.return_value.hexdigest.return_value = guard.SOURCE_SHA256
            code = guard.patch('    LOG_DBG("x/y: %d/%d", x, y);')
        harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define BIT(n) (1U << (n))
#define LOG_WRN(...) ((void)0)
#define LOG_INF(...) ((void)0)
#define LOG_DBG(...) ((void)0)
struct state { int dx, dy; };
static int check(uint8_t motion, bool ff, int16_t x, int16_t y, struct state *data) {
    uint8_t buf[7] = {motion, 0, 0, 0, 20, 0, 0};
    if (ff) for (int i = 0; i < 7; i++) buf[i] = 0xff;
    int64_t now = 2000;
''' + code + r'''
    return 1;
}
int main(void) {
    struct state s = {10, -20};
    assert(check(0, false, 100, -100, &s) == 0);
    assert(s.dx == 0 && s.dy == 0);
    s.dx = 10; s.dy = -20;
    assert(check(0xff, true, -1, -1, &s) == 0);
    assert(s.dx == 0 && s.dy == 0);
    assert(check(0x80, false, -100, 100, &s) == 1);
    assert(check(0x80, false, 100, -100, &s) == 1);
    assert(check(0x80, false, 0, 0, &s) == 1);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            src = Path(directory) / "guard.c"
            exe = Path(directory) / "guard"
            src.write_text(harness)
            subprocess.run(["cc", "-std=c11", "-Wall", "-Werror", str(src), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

