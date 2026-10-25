import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ManualMouseLayer(unittest.TestCase):
    def test_bottom_right_is_immediate_hold(self):
        text = (ROOT / 'config/SAA.keymap').read_text()
        layers = re.findall(r'^\s*bindings\s*=\s*<([^>]+)>', text, re.M)
        bindings = [re.findall(r'&\w+[^&]*', layer) for layer in layers]
        self.assertEqual(bindings[0][45].strip(), '&mo MOUSE')
        self.assertEqual(bindings[3][45].strip(), '&trans')
        self.assertEqual(len(bindings[0]), 58)

    def test_no_default_aml(self):
        text = (ROOT / 'boards/shields/SparAkashaAnanta/SAA_R_TB.overlay').read_text()
        self.assertIn('/delete-property/ temp-layer-enabled;', text)
