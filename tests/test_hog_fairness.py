import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("hog_patch", ROOT/"scripts/patch_hog_fairness.py")
hog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hog)

class HogFairness(unittest.TestCase):
    def test_unknown_revision_rejected(self):
        with self.assertRaises(ValueError):
            hog.patch("unknown")

    @unittest.skipUnless(os.environ.get("HOG_SOURCE") and shutil.which("cc"),
                         "set HOG_SOURCE to the pinned ZMK hog.c")
    def test_continuous_mouse_input_yields_to_keyboard(self):
        original = Path(os.environ["HOG_SOURCE"]).read_text()
        harness = Path(__file__).with_name("hog_fairness_harness.c").read_text()
        for source, budget in ((original,100), (hog.patch(original),1)):
            functions = []
            for kind in ("keyboard","consumer","mouse"):
                start = source.index(f"void send_{kind}_report_callback(")
                end = source.index(f"K_WORK_DEFINE(hog_{kind}_work",start)
                functions.append(source[start:end])
            with tempfile.TemporaryDirectory() as tmp:
                c, exe = Path(tmp)/"test.c", Path(tmp)/"test"
                c.write_text(harness.replace("/* CALLBACKS */","\n".join(functions)))
                subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",
                                "-Wno-unused-parameter","-Wno-unused-function",
                                f"-DBUDGET={budget}",str(c),"-o",str(exe)],check=True)
                subprocess.run([str(exe)],check=True)
