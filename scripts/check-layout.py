"""Check editor formatting indices without confusing them with GPIO matrix indices."""

import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
keys = json.loads((root / "config/SAA.json").read_text())["layouts"]["layout_SAA"]["layout"]
dts = (root / "boards/shields/SparAkashaAnanta/SAA_layout.dtsi").read_text()
attrs = [
    [int(n) for n in re.findall(r"-?\d+", value)]
    for value in re.findall(r"<&key_physical_attrs\s+([^>]+)>", dts)
]
assert len(keys) == len(attrs) == 58
# Matrix positions and keymap bindings group the five-way switches by hand.
# A visually row-sorted coordinate list can agree with JSON yet target wrong keys.
fiveway = [
    (675, 375), (725, 325), (725, 375), (725, 425), (775, 375),
    (875, 375), (925, 325), (925, 375), (925, 425), (975, 375),
]
assert [(p[2], p[3]) for p in attrs[46:56]] == fiveway, 'Five-way key position mismatch'
matrix = re.search(r'map\s*=\s*<([^>]+)>', dts).group(1)
rc = [(int(r), int(c)) for r, c in re.findall(r'RC\(\s*(\d+),\s*(\d+)\)', matrix)]
assert rc[46:56] == [(3, 5), (2, 5), (4, 5), (1, 5), (0, 5),
                     (1, 11), (3, 11), (4, 11), (0, 11), (2, 11)]
previous = (-1, -1)
for index, (key, physical) in enumerate(zip(keys, attrs)):
    position = (key["row"], key["col"])
    assert all(isinstance(n, int) and n >= 0 for n in position), index
    assert position > previous, f"Non-sequential editor position at key {index}"
    previous = position
    geometry = [round(key.get(name, default) * 100) for name, default in
                [("w", 1), ("h", 1), ("x", 0), ("y", 0), ("r", 0), ("rx", 0), ("ry", 0)]]
    assert geometry == physical, f"JSON/DTS geometry differs at key {index}"

# Editor row/col determine serialized binding order, not electrical pin locations.
ordered = sorted(range(len(keys)), key=lambda i: (keys[i]["row"], keys[i]["col"]))
assert ordered == list(range(58)), "Saving in the editor would reorder bindings"
keymap = (root / "config/SAA.keymap").read_text()
layers = re.findall(r"^\s*bindings\s*=\s*<([^>]+)>", keymap, re.M)
assert layers
for index, layer in enumerate(layers):
    assert len(re.findall(r"&\w+", layer)) == 58, f"Wrong binding count in layer {index}"
print(f"PASS: 58 keys, {len(layers)} layers, sequential editor positions, unchanged binding order, matching DTS geometry")
