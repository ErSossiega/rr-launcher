#!/usr/bin/env python3
"""
Converts the main menu tile artwork in assets/Images into the files embedded in the channel.

Each `<name>_<number>.png` (any size, 16:9) becomes `data/tile_<name>`: a 256x144 PNG without an
extension (the Makefile only links extensionless files in data/ through incbin.S). The menu resizes
it to the actual tile size once when it opens, so the full-size originals never reach the Wii,
where a decoded 1920x1080 image alone would take 8 MB.

Requires Pillow: pip install pillow
Usage (from the repository root): python tools/make_tiles.py
"""

import pathlib
import re

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE_DIR = ROOT / "assets" / "Images"
DATA_DIR = ROOT / "data"
SIZE = (256, 144)

for source in sorted(SOURCE_DIR.glob("*.png")):
    name = re.sub(r"_\d+$", "", source.stem).lower()
    target = DATA_DIR / f"tile_{name}"

    image = Image.open(source).convert("RGB").resize(SIZE, Image.LANCZOS)
    image.save(target, format="PNG", optimize=True)
    print(f"{source.relative_to(ROOT)} -> {target.relative_to(ROOT)} ({target.stat().st_size} bytes)")
