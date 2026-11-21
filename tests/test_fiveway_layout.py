import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class FiveWayLayout(unittest.TestCase):
    def test_editor_and_studio_positions(self):
        subprocess.run([sys.executable, str(ROOT / 'scripts/check-layout.py')], check=True)


if __name__ == '__main__':
    unittest.main()
