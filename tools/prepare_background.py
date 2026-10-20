"""Encode a user picture as the 800x480 RGB565 LE flash asset (requires Pillow).

The firmware consumes the committed asset directly; normal builds need no Pillow.
Only fit/encoding are applied. The source picture is never overwritten.
"""
import argparse
from array import array
import hashlib
from pathlib import Path
import sys

from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parent.parent
WIDTH, HEIGHT = 800, 480


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path,
                        default=ROOT / "main/assets/home_background.rgb565")
    parser.add_argument("--preview", type=Path, help="Optional preview of encoded pixels")
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve():
        parser.error("Source and output must be different files")
    if args.preview and args.preview.resolve() in (args.input.resolve(), args.output.resolve()):
        parser.error("Preview must not overwrite source or encoded asset")

    with Image.open(args.input) as original:
        source_size = original.size
        picture = ImageOps.exif_transpose(original).convert("RGB")
        fitted = ImageOps.fit(picture, (WIDTH, HEIGHT), Image.Resampling.LANCZOS,
                              centering=(0.5, 0.5))
    rgb = fitted.tobytes()
    words = array("H", (((rgb[i] >> 3) << 11) | ((rgb[i + 1] >> 2) << 5) |
                        (rgb[i + 2] >> 3) for i in range(0, len(rgb), 3)))
    if sys.byteorder != "little":
        words.byteswap()
    encoded = words.tobytes()
    assert len(encoded) == WIDTH * HEIGHT * 2
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(encoded)
    if args.preview:
        # Expand the actual 5/6/5 bits, so the preview shows stored pixel colours.
        decoded = bytearray()
        for i in range(0, len(encoded), 2):
            pixel = encoded[i] | encoded[i + 1] << 8
            red, green, blue = pixel >> 11, (pixel >> 5) & 63, pixel & 31
            decoded.extend(((red << 3) | (red >> 2), (green << 2) | (green >> 4),
                            (blue << 3) | (blue >> 2)))
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        Image.frombytes("RGB", (WIDTH, HEIGHT), bytes(decoded)).save(args.preview)
    print(f"Source: {source_size[0]}x{source_size[1]}; centre-cover fit: {WIDTH}x{HEIGHT}")
    print(f"RGB565 LE: {len(encoded)} bytes; stride: {WIDTH * 2}")
    print("Source SHA256:", hashlib.sha256(args.input.read_bytes()).hexdigest())
    print("Asset SHA256:", hashlib.sha256(encoded).hexdigest())


if __name__ == "__main__":
    main()
