import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ManualMouseLayer(unittest.TestCase):
    def test_bottom_right_is_layer_tap_and_click_positions(self):
        text = (ROOT / 'config/SAA.keymap').read_text()
        layers = re.findall(r'^\s*bindings\s*=\s*<([^>]+)>', text, re.M)
        bindings = [re.findall(r'&\w+[^&]*', layer) for layer in layers]
        self.assertEqual(bindings[0][45].strip(), '&lt MOUSE RIGHT')
        self.assertEqual(bindings[3][45].strip(), '&trans')
        self.assertEqual(len(bindings[0]), 58)
        self.assertEqual([bindings[3][i].strip() for i in (32, 33, 34)],
                         ['&mkp MB1', '&mkp MB3', '&mkp MB2'])
        self.assertEqual([bindings[3][i].strip() for i in (19, 20, 21)],
                         ['&trans', '&trans', '&trans'])

    def test_no_default_aml(self):
        text = (ROOT / 'boards/shields/SparAkashaAnanta/SAA_R_TB.overlay').read_text()
        self.assertIn('/delete-property/ temp-layer-enabled;', text)
