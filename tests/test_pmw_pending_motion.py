import shutil
import subprocess
import tempfile
from pathlib import Path
import unittest
from test_pmw_guard import guard


class PendingMotion(unittest.TestCase):
    def test_unknown_source_rejected(self):
        with self.assertRaises(ValueError):
            guard.preserve_pending_motion('unknown')

    @unittest.skipUnless(shutil.which('cc'), 'host C compiler required')
    def test_delayed_samples_preserve_signed_movement(self):
        block = '''    if (report_interval_min_ms > 0) {
        // purge accumulated delta, if last sampled had not been reported on last report tick
        if (now - data->last_smp_time >= report_interval_min_ms) {
            data->dx = 0;
            data->dy = 0;
        }
        data->last_smp_time = now;
    }
    data->dx += x;
    data->dy += y;
    if (report_interval_min_ms > 0 && now - data->last_rpt_time < report_interval_min_ms) {
        return;
    }
'''
        code = r'''
#include <stdint.h>
#include <assert.h>
struct state { int32_t dx, dy; int64_t last_smp_time, last_rpt_time; };
static int total_x, total_y;
static void step(struct state *data, int64_t now, int x, int y, unsigned report_interval_min_ms) {
''' + guard.preserve_pending_motion(block) + r'''
    total_x += data->dx; total_y += data->dy;
    data->dx = data->dy = 0; data->last_rpt_time = now;
}
int main(void) {
    struct state s = {0};
    step(&s, 5, 10, -7, 14);
    assert(total_x == 0 && s.dx == 10);
    step(&s, 25, 1, -2, 14);
    assert(total_x == 11 && total_y == -9 && s.dx == 0);
    step(&s, 26, -3, 4, 14);
    step(&s, 27, -2, 1, 14);
    assert(total_x == 11 && s.dx == -5);
    step(&s, 40, 0, 0, 14);
    assert(total_x == 6 && total_y == -4 && s.dx == 0);
    step(&s, 41, 2, -2, 0);
    assert(total_x == 8 && total_y == -6);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            src, exe = Path(directory) / 'test.c', Path(directory) / 'test'
            src.write_text(code)
            subprocess.run(['cc', '-std=c11', '-Wall', '-Werror', str(src), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
