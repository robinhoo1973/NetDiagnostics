#!/usr/bin/env python3
"""Generate iPad 13" screenshots from 6.1" iPhone source images."""
from pathlib import Path
from PIL import Image

PROJECT = Path(__file__).resolve().parent.parent
SRC_DIR = PROJECT / "doc" / "screenshot" / "6.1"

# iPad 13" (M4): 2064 x 2752, aspect ratio ~3:4
IPAD_W, IPAD_H = 2064, 2752

OUT_DIR = PROJECT / "doc" / "screenshot" / "ipad-13"
OUT_DIR.mkdir(parents=True, exist_ok=True)

for src_path in sorted(SRC_DIR.glob("*.png")):
    img = Image.open(src_path)
    sw, sh = img.size  # 1125 x 2436

    # Scale to fill iPad height, then center-crop width
    scale = IPAD_H / sh
    new_w = int(sw * scale)
    new_h = IPAD_H

    scaled = img.resize((new_w, new_h), Image.LANCZOS)

    # Center-crop to exact iPad size
    left = (new_w - IPAD_W) // 2
    cropped = scaled.crop((left, 0, left + IPAD_W, IPAD_H))

    dst_path = OUT_DIR / src_path.name
    cropped.save(dst_path, "PNG")
    print(f"  OK  {src_path.name}  {sw}x{sh} -> {IPAD_W}x{IPAD_H}")

print(f"\nGenerated {len(list(OUT_DIR.glob('*.png')))} iPad screenshots")
