#!/usr/bin/env python3
"""Turns your image into logo.png, the icon Geode shows for the mod.

Add your image to the repo root as logo-source.webp (or .png / .jpg). It can also keep its own
name, as long as it is the only image there. This script then:

  1. crops to the icon (the plain light margin around it is dropped),
  2. makes the area outside the rounded corners transparent,
  3. pads to a square (nothing is stretched) and scales to 336 x 336,
  4. writes logo.png.

The artwork itself is not redrawn or recoloured. If logo.png already exists it is left alone, so a
logo you made yourself always wins. With no image at all the build goes on without a logo.

    python tools/make_logo.py [image] [--out logo.png] [--size 336] [--force]
"""
import argparse
import pathlib
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageOps

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXTS = {".webp", ".png", ".jpg", ".jpeg"}
TOL = 40  # how far from the background colour a pixel can be and still count as background


def warn(text):
    print(f"::warning title=Alt Flow logo::{text}")


def find_source():
    """(image to use, None) or (None, why there is none)."""
    for stem in ("logo-source", "logo_source"):
        for ext in sorted(EXTS):
            path = ROOT / (stem + ext)
            if path.is_file():
                return path, None
    others = sorted(p for p in ROOT.iterdir() if p.is_file() and p.suffix.lower() in EXTS and p.name.lower() != "logo.png")
    if len(others) == 1:
        return others[0], None
    if len(others) > 1:
        return None, "more than one image in the repo root (" + ", ".join(p.name for p in others) + "); rename the logo one to logo-source.webp"
    return None, "no logo yet: add your image to the repo as logo-source.webp (or .png / .jpg) and build again"


def cut_out(img):
    """Return (icon as RGBA with transparent surroundings, short note on what was done)."""
    w, h = img.size

    # an image that already has transparency: trust it, just crop to what is visible
    if img.getchannel("A").getextrema()[0] < 250:
        box = img.getchannel("A").point(lambda a: 255 if a > 8 else 0).getbbox()
        return (img.crop(box) if box else img), "kept the existing transparency"

    px = img.load()
    corners = [px[0, 0], px[w - 1, 0], px[0, h - 1], px[w - 1, h - 1]]
    bg = tuple(sorted(c[i] for c in corners)[1] for i in range(3))
    spread = max(abs(c[i] - bg[i]) for c in corners for i in range(3))
    if min(bg) < 200 or spread > TOL:
        # corners are not a plain light margin (already a tight crop?): use the picture as it is
        return img, "no plain light margin found, used the whole image"

    # how far every pixel is from the background colour (largest difference of any channel)
    rgb = img.convert("RGB")
    r, g, b = ImageChops.difference(rgb, Image.new("RGB", rgb.size, bg)).split()
    dist = ImageChops.lighter(ImageChops.lighter(r, g), b)
    box = dist.point(lambda v: 255 if v > TOL else 0).getbbox()
    if box is None:
        return img, "image is blank, used it as it is"
    m = 2
    box = (max(box[0] - m, 0), max(box[1] - m, 0), min(box[2] + m, w), min(box[3] + m, h))
    crop = img.crop(box)
    dist = dist.crop(box)

    # background that touches the crop's corners = outside the rounded square
    flood = dist.point(lambda v: 255 if v <= TOL else 0)  # 255 = looks like background
    cw, ch = flood.size
    for xy in ((0, 0), (cw - 1, 0), (0, ch - 1), (cw - 1, ch - 1)):
        if flood.getpixel(xy) == 255:
            ImageDraw.floodfill(flood, xy, 128)
    outside = flood.point(lambda v: 255 if v == 128 else 0)
    share = outside.histogram()[255] / float(cw * ch)
    if share > 0.35:
        return crop, f"background removal looked wrong ({share:.0%} would go), kept the crop as it is"

    # drop one more pixel inward so no light fringe from the old background is left on the edge
    inside = ImageChops.invert(outside).filter(ImageFilter.MinFilter(3))
    out = crop.copy()
    out.putalpha(inside)
    return out.crop(inside.getbbox()), f"cropped, made the outside transparent ({share:.1%} of the crop)"


def to_square(img):
    w, h = img.size
    side = max(w, h)
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    canvas.paste(img, ((side - w) // 2, (side - h) // 2))
    return canvas


def main():
    ap = argparse.ArgumentParser(description="Make logo.png from your image")
    ap.add_argument("image", nargs="?", help="source image (default: look in the repo root)")
    ap.add_argument("--out", default=str(ROOT / "logo.png"))
    ap.add_argument("--size", type=int, default=336)
    ap.add_argument("--force", action="store_true", help="overwrite an existing logo.png")
    args = ap.parse_args()

    out = pathlib.Path(args.out)
    if out.exists() and not args.force:
        print(f"logo: {out.name} is already there, leaving it alone")
        return 0

    if args.image:
        source, problem = pathlib.Path(args.image), None
    else:
        source, problem = find_source()
    if source is None:
        warn(problem)
        return 0

    try:
        img = ImageOps.exif_transpose(Image.open(source)).convert("RGBA")
    except Exception as e:  # unreadable image: the mod still builds, just without an icon
        warn(f"could not read {source.name}: {e}")
        return 0

    icon, note = cut_out(img)
    icon = to_square(icon).resize((args.size, args.size), Image.LANCZOS)
    icon.putalpha(icon.getchannel("A").point(lambda a: 0 if a < 4 else a))  # drop resampling specks
    icon.save(out, "PNG", optimize=True)
    print(f"logo: {source.name} {img.size[0]}x{img.size[1]} -> {out.name} {args.size}x{args.size} ({note})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
