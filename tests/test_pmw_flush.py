"""Execute the generated production report/callback against a mocked sensor."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from test_pmw_guard import guard

@unittest.skipUnless(os.environ.get("PMW_SOURCE") and shutil.which("cc"),
                     "set PMW_SOURCE to the pinned driver's pmw3610.c")
class MotionFlush(unittest.TestCase):
    def test_real_report_and_scheduled_callback(self):
        source = Path(os.environ["PMW_SOURCE"]).read_text()
        source = guard.retain_rejected_input(guard.preserve_pending_motion(guard.isolate_motion_work(guard.patch(source))))
        report = source[source.index("static int pmw3610_report_data("):
                        source.index("static void pmw3610_gpio_callback(")]
        # Queue declarations between functions are supplied by the harness.
        report = report[:report.index("/* Ananta comparison:")]
        callback = source[source.index("static void pmw3610_work_callback("):
                          source.index("static int pmw3610_enable_irq(")]
        harness = (Path(__file__).with_name("pmw_flush_harness.c")).read_text()
        with tempfile.TemporaryDirectory() as tmp:
            c, exe = Path(tmp)/"test.c", Path(tmp)/"test"
            c.write_text(harness.replace("/* PRODUCTION_FUNCTIONS */", report + callback))
            subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                            str(c), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
