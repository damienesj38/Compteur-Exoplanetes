"""Convertit les images RGB565 du simulateur en PNG."""
import sys
from pathlib import Path
from PIL import Image

for f in sorted(Path(sys.argv[1] if len(sys.argv) > 1 else "out").glob("*.rgb565")):
    data = f.read_bytes()
    img = Image.new("RGB", (800, 480))
    px = img.load()
    for i in range(800 * 480):
        v = data[2 * i] | (data[2 * i + 1] << 8)
        # readPixel renvoie déjà du RGB565 dans l ordre naturel.
        pass
        r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
        px[i % 800, i // 800] = (r * 255 // 31, g * 255 // 63, b * 255 // 31)
    img.save(f.with_suffix(".png"))
    print(f.with_suffix(".png"))
