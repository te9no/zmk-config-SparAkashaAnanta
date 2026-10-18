from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SHIELDS = ROOT / "boards/shields/SparAkashaAnanta"


class Diagnostics(unittest.TestCase):
    def test_cdc_does_not_enable_hot_path_debug_logging(self):
        conf = (SHIELDS / "SAA_R_Base.conf").read_text()
        self.assertIn("CONFIG_ZMK_USB_LOGGING=y", conf)
        self.assertIn("CONFIG_ZMK_LOG_LEVEL_INF=y", conf)
        self.assertIn("CONFIG_ZMK_LOGGING_MINIMAL=y", conf)

    def test_central_rpc_features(self):
        conf = (SHIELDS / "SAA_R_Base.conf").read_text()
        for name in ("DEVICE_INFO", "WATCHDOG", "KSCAN_DIAGNOSTICS", "DEVTOOL"):
            self.assertIn(f"CONFIG_ZMK_{name}=y", conf)
            self.assertIn(f"CONFIG_ZMK_{name}_STUDIO_RPC=y", conf)
        self.assertIn("CONFIG_ZMK_DEVTOOL_STACK_USAGE=y", conf)

    def test_peripheral_uses_relay_not_local_studio_rpc(self):
        conf = (SHIELDS / "SAA_L_Base.conf").read_text()
        self.assertIn("CONFIG_ZMK_KSCAN_DIAGNOSTICS_STUDIO_RPC=n", conf)
        self.assertIn("CONFIG_ZMK_KSCAN_DIAGNOSTICS=y", conf)

    def test_both_halves_have_matching_relay_and_no_freeze_timer(self):
        for side in ("L", "R"):
            conf = (SHIELDS / f"SAA_{side}_Base.conf").read_text()
            self.assertIn("CONFIG_ZMK_SPLIT_RELAY_EVENT_DATA_LEN=256", conf)
            self.assertIn("CONFIG_ZMK_WATCHDOG_SPLIT_RELAY=y", conf)
            self.assertIn("CONFIG_ZMK_WATCHDOG_FREEZE_DETECT=n", conf)
