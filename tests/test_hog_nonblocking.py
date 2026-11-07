import importlib.util
from pathlib import Path
import os
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("nb",ROOT/"scripts/patch_hog_nonblocking.py")
nb=importlib.util.module_from_spec(spec);spec.loader.exec_module(nb)
class NonblockingPatch(unittest.TestCase):
    def test_unreviewed_revision_rejected(self):
        with self.assertRaises(ValueError):nb.patch("unknown")
    @unittest.skipUnless(os.environ.get("HOG_SOURCE"),"set HOG_SOURCE")
    def test_service_preserved_and_production_dispatch_replaced(self):
        original=Path(os.environ["HOG_SOURCE"]).read_text()
        patched=nb.patch(original)
        marker="K_THREAD_STACK_DEFINE(hog_q_stack"
        self.assertTrue(patched.startswith(original[:original.index(marker)]))
        dispatch=patched[len(original[:original.index(marker)]):]
        self.assertNotIn("K_FOREVER",dispatch)
        self.assertNotIn("K_MSEC(100)",dispatch)
        self.assertNotIn("k_msgq_put",dispatch)
        self.assertIn("k_work_reschedule(&saa_send_work",dispatch)
        for kind in ("keyboard","consumer","mouse"):
            self.assertEqual(dispatch.count(f"int zmk_hog_send_{kind}_report("),1)
