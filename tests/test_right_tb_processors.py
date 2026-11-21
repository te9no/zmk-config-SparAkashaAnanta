from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RightTrackball(unittest.TestCase):
    def test_runtime_and_scroll_paths(self):
        source = (ROOT / "boards/shields/SparAkashaAnanta/SAA_R_TB.overlay").read_text()
        self.assertIn("temp-layer = <3>", source)
        self.assertIn("temp-layer-enabled;", source)
        listener = source.split("trackball_listener: trackball_listener", 1)[1]
        base, scroll = listener.split("scroll {", 1)
        self.assertIn("<&mouse_runtime_input_processor>", base)
        self.assertIn("layers = <1 4>", scroll)
        self.assertIn("<&zip_xy_to_scroll_mapper>", scroll)
        self.assertIn("<&scroll_runtime_input_processor>", scroll)
        self.assertNotIn("<&mouse_runtime_input_processor>", scroll)
        self.assertNotIn("process-next", scroll)


if __name__ == "__main__":
    unittest.main()
