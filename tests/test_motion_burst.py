from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MotionBurst(unittest.TestCase):
    def test_right_tb_uses_burst_without_changing_wiring(self):
        overlay = (ROOT / 'boards/shields/SparAkashaAnanta/SAA_R_TB.overlay').read_text()
        self.assertNotIn('disable-burst-read;', overlay)
        self.assertIn('compatible = "te9no,spi-three-wire-gpio";', overlay)
        self.assertIn('sdio-gpios = P0_03_GPIOS(GPIO_ACTIVE_HIGH);', overlay)
        self.assertIn('settings-id = "saa-rtb";', overlay)
