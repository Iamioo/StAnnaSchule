"""Turns the face BMP rendered by `StAnna.exe -icon` into icons.

usage: make_icon.py face.bmp out.ico [android_res_dir]
Pure black is the transparent background (the art itself never uses it).
"""
import sys
from pathlib import Path
from PIL import Image

face = Image.open(sys.argv[1]).convert("RGBA")
px = face.load()
for y in range(face.height):
    for x in range(face.width):
        r, g, b, _ = px[x, y]
        if r == 0 and g == 0 and b == 0:
            px[x, y] = (0, 0, 0, 0)

sizes = [16, 24, 32, 48, 64, 128]
face.save(sys.argv[2], sizes=[(s, s) for s in sizes])

if len(sys.argv) > 3:
    res = Path(sys.argv[3])
    for name, size in (("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)):
        d = res / f"mipmap-{name}"
        d.mkdir(parents=True, exist_ok=True)
        # dark rounded backdrop so the bald head reads on any launcher
        icon = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        back = Image.new("RGBA", (size, size), (0x14, 0x10, 0x1C, 255))
        mask = Image.new("L", (size, size), 0)
        from PIL import ImageDraw
        ImageDraw.Draw(mask).rounded_rectangle([0, 0, size - 1, size - 1], radius=size // 5, fill=255)
        icon.paste(back, (0, 0), mask)
        f = face.resize((size * 13 // 16, size * 13 // 16), Image.NEAREST)
        icon.alpha_composite(f, ((size - f.width) // 2, (size - f.height) // 2))
        icon.save(d / "ic_launcher.png")
print("icons written")
