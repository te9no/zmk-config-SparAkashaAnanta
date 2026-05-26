#!/usr/bin/env python3
import argparse
import html
import math
import re
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Key:
    width: float
    height: float
    x: float
    y: float
    rotation: float
    rx: float
    ry: float
    row: int | None = None
    col: int | None = None


@dataclass
class Binding:
    tap: str
    hold: str = ""
    css_class: str = ""


@dataclass
class Layer:
    name: str
    label: str
    bindings: list[Binding]


def find_labeled_block(text, label):
    match = re.search(rf"\b{re.escape(label)}\s*:\s*[A-Za-z0-9_,@-]+\s*\{{", text)
    if not match:
        raise SystemExit(f"Could not find node label: {label}")

    start = match.end()
    depth = 1
    index = start
    while index < len(text) and depth:
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
        index += 1

    if depth:
        raise SystemExit(f"Unclosed node block for label: {label}")
    return text[start : index - 1]


def find_named_block(text, name):
    match = re.search(rf"\b{re.escape(name)}\s*\{{", text)
    if not match:
        raise SystemExit(f"Could not find node: {name}")

    start = match.end()
    depth = 1
    index = start
    while index < len(text) and depth:
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
        index += 1

    if depth:
        raise SystemExit(f"Unclosed node block for: {name}")
    return text[start : index - 1]


def iter_child_blocks(text):
    index = 0
    pattern = re.compile(r"\b([A-Za-z0-9_]+)\s*\{")
    while True:
        match = pattern.search(text, index)
        if not match:
            break

        name = match.group(1)
        start = match.end()
        depth = 1
        cursor = start
        while cursor < len(text) and depth:
            if text[cursor] == "{":
                depth += 1
            elif text[cursor] == "}":
                depth -= 1
            cursor += 1

        if depth:
            raise SystemExit(f"Unclosed node block for: {name}")
        yield name, text[start : cursor - 1]
        index = cursor


def find_first_physical_layout_label(text):
    for match in re.finditer(r"\b([A-Za-z0-9_]+)\s*:\s*[A-Za-z0-9_,@-]+\s*\{", text):
        block = find_labeled_block(text, match.group(1))
        if re.search(r'compatible\s*=\s*"zmk,physical-layout"', block):
            return match.group(1)
    raise SystemExit("Could not find a zmk,physical-layout node")


def clean_number(value):
    number = int(value.strip("()")) if isinstance(value, str) else value
    return number / 100


def parse_display_name(layout_block, fallback):
    match = re.search(r'\bdisplay-name\s*=\s*"([^"]+)"\s*;', layout_block)
    return match.group(1) if match else fallback


def parse_key_attrs(layout_block):
    keys_match = re.search(r"\bkeys\b.*?=\s*(.*?);", layout_block, re.S)
    if not keys_match:
        raise SystemExit("Could not find physical layout keys")

    keys = []
    for match in re.finditer(
        r"<\s*&key_physical_attrs\s+(\(?-?\d+\)?)\s+(\(?-?\d+\)?)\s+(\(?-?\d+\)?)\s+(\(?-?\d+\)?)\s+(\(?-?\d+\)?)\s+(\(?-?\d+\)?)\s+(\(?-?\d+\)?)\s*>",
        keys_match.group(1),
    ):
        width, height, x, y, rotation, rx, ry = (
            clean_number(value) for value in match.groups()
        )
        keys.append(Key(width, height, x, y, rotation, rx, ry))

    if not keys:
        raise SystemExit("No key_physical_attrs entries found")
    return keys


def parse_transform_positions(text, layout_block):
    transform_match = re.search(r"transform\s*=\s*<\s*&([A-Za-z0-9_]+)\s*>", layout_block)
    if not transform_match:
        return []

    transform_block = find_labeled_block(text, transform_match.group(1))
    map_match = re.search(r"\bmap\b\s*=\s*<(.*?)>;", transform_block, re.S)
    if not map_match:
        return []

    return [
        (int(row), int(col))
        for row, col in re.findall(r"RC\(\s*(\d+)\s*,\s*(\d+)\s*\)", map_match.group(1))
    ]


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//.*", "", text)


def parse_binding_groups(bindings_text):
    tokens = strip_comments(bindings_text).replace("<", " ").replace(">", " ").split()
    groups = []
    current = []
    for token in tokens:
        if token.startswith("&"):
            if current:
                groups.append(current)
            current = [token]
        elif current:
            current.append(token)
    if current:
        groups.append(current)
    return groups


def friendly_keycode(value):
    aliases = {
        "BACKSPACE": "BKSP",
        "RETURN": "ENTER",
        "ESCAPE": "ESC",
        "DELETE": "DEL",
        "MINUS": "-",
        "EQUAL": "=",
        "LBKT": "[",
        "RBKT": "]",
        "BSLH": "\\",
        "FSLH": "/",
        "SEMI": ";",
        "SQT": "'",
        "GRAVE": "`",
        "COMMA": ",",
        "DOT": ".",
        "SPACE": "SPC",
    }
    if re.fullmatch(r"N\d", value):
        return value[1]
    return aliases.get(value, value)


def format_binding(group):
    if not group:
        return Binding("")

    behavior = group[0].removeprefix("&")
    args = group[1:]
    if behavior == "trans":
        return Binding("TRANS", css_class="transparent")
    if behavior == "none":
        return Binding("NONE", css_class="none")
    if behavior == "kp":
        return Binding(friendly_keycode(args[0]) if args else "")
    if behavior == "mkp":
        return Binding(friendly_keycode(args[0]) if args else "", "MOUSE")
    if behavior == "mt":
        tap = friendly_keycode(args[-1]) if args else ""
        hold = "+".join(friendly_keycode(arg) for arg in args[:-1])
        return Binding(tap, hold)
    if behavior == "lt":
        layer = args[0] if args else ""
        tap = friendly_keycode(args[1]) if len(args) > 1 else ""
        return Binding(tap, f"LT {layer}")
    if behavior in {"mo", "to", "tog"}:
        return Binding(f"{behavior.upper()} {args[0]}" if args else behavior.upper())
    if behavior == "bt":
        return Binding(" ".join(args), "BT")
    if args:
        return Binding(" ".join(friendly_keycode(arg) for arg in args), behavior)
    return Binding(behavior)


def parse_keymap_layers(keymap_path):
    keymap_text = keymap_path.read_text()
    keymap_block = find_named_block(keymap_text, "keymap")
    layers = []
    for name, block in iter_child_blocks(keymap_block):
        bindings_match = re.search(r"\bbindings\b\s*=\s*<(.*?)>;", block, re.S)
        if not bindings_match:
            continue
        label_match = re.search(r'\blabel\s*=\s*"([^"]+)"\s*;', block)
        label = label_match.group(1) if label_match else name
        groups = parse_binding_groups(bindings_match.group(1))
        layers.append(Layer(name, label, [format_binding(group) for group in groups]))

    if not layers:
        raise SystemExit(f"No keymap layers with bindings found in {keymap_path}")
    return layers


def apply_positions(keys, positions):
    if not positions:
        return
    if len(positions) != len(keys):
        raise SystemExit(
            f"Transform position count ({len(positions)}) does not match key count ({len(keys)})"
        )
    for key, (row, col) in zip(keys, positions):
        key.row = row
        key.col = col


def rotate_point(x, y, angle_degrees, origin_x, origin_y):
    if angle_degrees == 0:
        return x, y
    angle = math.radians(angle_degrees)
    dx = x - origin_x
    dy = y - origin_y
    return (
        origin_x + dx * math.cos(angle) - dy * math.sin(angle),
        origin_y + dx * math.sin(angle) + dy * math.cos(angle),
    )


def key_corners(key):
    points = [
        (key.x, key.y),
        (key.x + key.width, key.y),
        (key.x + key.width, key.y + key.height),
        (key.x, key.y + key.height),
    ]
    return [rotate_point(x, y, key.rotation, key.rx, key.ry) for x, y in points]


def layout_bounds(keys):
    points = [point for key in keys for point in key_corners(key)]
    min_x = min(x for x, _ in points)
    min_y = min(y for _, y in points)
    max_x = max(x for x, _ in points)
    max_y = max(y for _, y in points)
    return min_x, min_y, max_x, max_y


def px(value):
    rounded = round(value, 3)
    if rounded == int(rounded):
        return str(int(rounded))
    return f"{rounded:.3f}".rstrip("0").rstrip(".")


def transform_key(key, min_x, min_y, scale, padding_x, padding_y):
    x = (key.x - min_x) * scale + padding_x
    y = (key.y - min_y) * scale + padding_y
    rx = (key.rx - min_x) * scale + padding_x
    ry = (key.ry - min_y) * scale + padding_y
    return x, y, rx, ry


def render_key_text(binding, key_width, key_height):
    if binding is None:
        return []

    tap = html.escape(binding.tap)
    hold = html.escape(binding.hold)
    lines = [
        f'<text class="tap" x="{px(key_width / 2)}" y="{px(key_height / 2 - (6 if hold else 0))}">{tap}</text>'
    ]
    if hold:
        lines.append(
            f'<text class="hold" x="{px(key_width / 2)}" y="{px(key_height / 2 + 11)}">{hold}</text>'
        )
    return lines


def render_svg(keys, title, output_path, key_unit_px, padding, show_row_col, layers):
    min_x, min_y, max_x, max_y = layout_bounds(keys)
    header_height = 34
    layout_width = (max_x - min_x) * key_unit_px + padding * 2
    layout_height = (max_y - min_y) * key_unit_px + padding * 2 + header_height
    layer_gap = 42
    rendered_layers = layers or [Layer("physical_layout", title, [])]
    width = layout_width
    height = layout_height * len(rendered_layers) + layer_gap * (len(rendered_layers) - 1)

    lines = [
        f'<svg width="{px(width)}" height="{px(height)}" viewBox="0 0 {px(width)} {px(height)}" class="physical-layout" xmlns="http://www.w3.org/2000/svg">',
        "<style>",
        "svg.physical-layout { font-family: ui-monospace, SFMono-Regular, Consolas, monospace; background: #f8faf7; color: #1d2520; }",
        ".key rect { fill: #ffffff; stroke: #8a958d; stroke-width: 1.25; }",
        ".key text { text-anchor: middle; dominant-baseline: middle; fill: #1d2520; }",
        ".key .tap { font-size: 11px; font-weight: 700; }",
        ".key .hold, .key .matrix { font-size: 7.5px; fill: #69736c; }",
        ".key.transparent rect { fill: #f3f5f2; stroke-dasharray: 4 3; }",
        ".key.transparent text { fill: #9aa39d; }",
        ".key.none rect { fill: #ecefed; }",
        ".title { font-size: 16px; font-weight: 700; text-anchor: start; dominant-baseline: hanging; fill: #1d2520; }",
        ".subtitle { font-size: 10px; fill: #69736c; text-anchor: start; dominant-baseline: hanging; }",
        ".origin { fill: #d1495b; opacity: 0.75; }",
        "</style>",
        f'<title>{html.escape(title)}</title>',
    ]

    for layer_index, layer in enumerate(rendered_layers):
        y_offset = layer_index * (layout_height + layer_gap)
        lines.append(f'<g class="layer layer-{html.escape(layer.name)}" transform="translate(0 {px(y_offset)})">')
        lines.append(
            f'<text class="title" x="{px(padding)}" y="{px(padding / 3)}">{html.escape(layer.label)}</text>'
        )
        if layer.bindings:
            lines.append(
                f'<text class="subtitle" x="{px(padding)}" y="{px(padding / 3 + 18)}">{len(layer.bindings)} bindings</text>'
            )
        for index, key in enumerate(keys):
            x, y, rx, ry = transform_key(key, min_x, min_y, key_unit_px, padding, padding + header_height)
            key_width = key.width * key_unit_px
            key_height = key.height * key_unit_px
            group_transform = (
                f'translate({px(x)} {px(y)}) rotate({px(key.rotation)} {px(rx - x)} {px(ry - y)})'
            )
            binding = layer.bindings[index] if index < len(layer.bindings) else None
            css_class = binding.css_class if binding else ""
            matrix_label = f"r{key.row} c{key.col}" if key.row is not None and key.col is not None else ""
            lines.extend(
                [
                    f'<g class="key keypos-{index} {css_class}" transform="{group_transform}">',
                    f'<rect x="0" y="0" width="{px(key_width)}" height="{px(key_height)}" rx="6" ry="6"/>',
                ]
            )
            if binding:
                lines.extend(render_key_text(binding, key_width, key_height))
            else:
                lines.append(
                    f'<text class="tap" x="{px(key_width / 2)}" y="{px(key_height / 2 - (6 if show_row_col and matrix_label else 0))}">{index}</text>'
                )
            if show_row_col and matrix_label and not binding:
                lines.append(
                    f'<text class="matrix" x="{px(key_width / 2)}" y="{px(key_height / 2 + 10)}">{matrix_label}</text>'
                )
            lines.append("</g>")
            if key.rotation and not layer.bindings:
                lines.append(f'<circle class="origin" cx="{px(rx)}" cy="{px(ry)}" r="2.5"/>')
        lines.append("</g>")

    lines.append("</svg>")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines) + "\n")


def generate(input_path, output_path, layout_name, keymap_path, key_unit_px, padding, show_row_col):
    text = input_path.read_text()
    layout_label = layout_name or find_first_physical_layout_label(text)
    layout_block = find_labeled_block(text, layout_label)
    keys = parse_key_attrs(layout_block)
    apply_positions(keys, parse_transform_positions(text, layout_block))
    title = parse_display_name(layout_block, layout_label)
    layers = parse_keymap_layers(keymap_path) if keymap_path else []

    for layer in layers:
        if len(layer.bindings) != len(keys):
            raise SystemExit(
                f"Layer {layer.label} binding count ({len(layer.bindings)}) does not match key count ({len(keys)})"
            )

    render_svg(keys, title, output_path, key_unit_px, padding, show_row_col, layers)


def main():
    parser = argparse.ArgumentParser(
        description="Generate an SVG preview from a ZMK zmk,physical-layout node and optional .keymap."
    )
    parser.add_argument("input", type=Path, help="ZMK .dtsi/.overlay containing zmk,physical-layout")
    parser.add_argument("output", type=Path, help="SVG file to write")
    parser.add_argument("--keymap", type=Path, help="ZMK .keymap file to draw on top of the physical layout")
    parser.add_argument("--layout", help="Physical layout node label, e.g. layout_SAA")
    parser.add_argument("--key-unit-px", type=float, default=52, help="Pixel size for a 1u key")
    parser.add_argument("--padding", type=float, default=30, help="SVG padding in pixels")
    parser.add_argument("--no-row-col", action="store_true", help="Hide matrix row/column labels")
    args = parser.parse_args()

    generate(
        args.input,
        args.output,
        args.layout,
        args.keymap,
        args.key_unit_px,
        args.padding,
        not args.no_row_col,
    )


if __name__ == "__main__":
    main()
