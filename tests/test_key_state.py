"""Check exact pinned production patch sites, not only the standalone harness."""
import importlib.util
import os
import sys
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
spec = importlib.util.spec_from_file_location("key_patch", ROOT / "scripts/patch_key_state.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class KeyStatePatch(unittest.TestCase):
    def test_revision_guard(self):
        for name in module.HASH:
            with self.subTest(name=name), self.assertRaises(ValueError):
                module.patch("unreviewed source", name)

    def sources(self):
        path = os.environ.get("HOG_SOURCE")
        if not path:
            self.skipTest("set HOG_SOURCE to the pinned, bridge-patched ZMK app/src/hog.c")
        return Path(path).parent

    def test_all_scan_and_central_queue_accesses_intercepted(self):
        root = self.sources()
        for relative, queue in (("physical_layouts.c", "physical_layouts_kscan_msgq"),
                                ("split/bluetooth/central.c", "peripheral_event_msgq")):
            path = root / relative
            original = path.read_text()
            patched = module.patch(original, path.name)
            for operation in ("put", "get"):
                old = f"k_msgq_{operation}(&{queue},"
                new = f"saa_state_{operation}(&{queue},"
                self.assertGreater(original.count(old), 0)
                self.assertEqual(patched.count(new), original.count(old))
                self.assertNotIn(old, patched)
            self.assertEqual(patched.count("struct saa_state_queue {"), 1)
            self.assertIn("return -EINVAL;", patched)

    def test_peripheral_preserves_other_services(self):
        path = self.sources() / "split/bluetooth/service.c"
        original = path.read_text()
        patched = module.patch(original, path.name)
        # Everything after the key-position functions, including relay/input
        # transports and service initialization, must remain byte-identical.
        start = original.index("K_MSGQ_DEFINE(sensor_state_msgq")
        self.assertEqual(patched[patched.index("K_MSGQ_DEFINE(sensor_state_msgq"):], original[start:])
        section = patched[patched.index("K_MSGQ_DEFINE(position_state_msgq"):patched.index("static int zmk_split_bt_position_pressed(")]
        self.assertNotIn("K_MSEC(100)", section)
        self.assertNotIn("return send_position_state();", section)
        self.assertIn("saa_position_subscription(value == BT_GATT_CCC_NOTIFY)", patched)
        self.assertEqual(patched.count("if (position >= POS_STATE_LEN * 8) return -EINVAL;"), 2)
