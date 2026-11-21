#!/usr/bin/env python3
"""Convert the Ninja Invasion artwork into in-game sprites.

Usage: python3 tools/make_ninja_art.py <folder with the three artwork PNGs>

Sources (in that folder):
  Jungle Ninja Victory Sprite Sheet.png   characters, story picture, background, platform
  Colourful Ninja Game Icon Asset Sheet.png   boss, sword, heart, coin
  Ninja Invasion Jungle Temple.png        title logo

Characters and icons are cropped to their content (ignoring the magenta #FF00FF
background), fitted into their box and padded with magenta, which is the
transparent colour. The story picture, background and title are opaque.

Writes art/NN-name.png for each picture, and src/ninja_art.c / .h with RGB565
arrays (0xF81F, magenta, is transparent in the sprites).
"""
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
VICTORY = "Jungle Ninja Victory Sprite Sheet.png"
ICONS = "Colourful Ninja Game Icon Asset Sheet.png"
TITLE = "Ninja Invasion Jungle Temple.png"
KEY = np.array([255, 0, 255], dtype=np.uint8)
KEY565 = 0xF81F


def is_key(a):
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    return (r > 180) & (g < 120) & (b > 180)


def resample(a, nw, nh):
    """Nearest-neighbour scaling: each output pixel copies one source pixel."""
    h, w = a.shape[:2]
    ys = np.arange(nh) * h // nh
    xs = np.arange(nw) * w // nw
    return a[ys][:, xs]


def fit_sprite(a, box, out_w, out_h):
    """Crop to the content (not magenta) inside box, fit it into out_w x out_h, pad with magenta."""
    x0, y0, x1, y1 = box
    src = a[y0:y1, x0:x1]
    k = ~is_key(src)
    ys, xs = np.where(k)
    src = src[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    ch, cw = src.shape[:2]
    scale = min(out_w / cw, out_h / ch)
    nw, nh = max(1, round(cw * scale)), max(1, round(ch * scale))
    small = resample(src, nw, nh)
    out = np.empty((out_h, out_w, 3), dtype=np.uint8)
    out[:] = KEY
    ox, oy = (out_w - nw) // 2, out_h - nh  # centred, feet on the bottom edge
    out[oy:oy + nh, ox:ox + nw] = small
    out[is_key(out)] = KEY
    return out


def cover_sprite(a, box, out_w, out_h, anchor="centre"):
    """Scale box to cover out_w x out_h (keeping aspect), then crop to size."""
    x0, y0, x1, y1 = box
    src = a[y0:y1, x0:x1]
    h, w = src.shape[:2]
    scale = max(out_w / w, out_h / h)
    nw, nh = max(out_w, round(w * scale)), max(out_h, round(h * scale))
    big = resample(src, nw, nh)
    cx = (nw - out_w) // 2
    cy = (nh - out_h) if anchor == "bottom" else (nh - out_h) // 2
    return big[cy:cy + out_h, cx:cx + out_w].copy()


def background_dimmed(a):
    """The temple picture, full screen, dimmed. Stray magenta from the sheet becomes dark teal."""
    bg = cover_sprite(a, (1071, 26, 1489, 326), 480, 320)
    bg[is_key(bg)] = (6, 24, 28)
    return (bg * 0.5).astype(np.uint8)


def stretch_sprite(a, box, out_w, out_h):
    x0, y0, x1, y1 = box
    return resample(a[y0:y1, x0:x1], out_w, out_h)


def rgb565(p):
    r, g, b = int(p[0]), int(p[1]), int(p[2])
    if r > 180 and g < 120 and b > 180:
        return KEY565
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    folder = sys.argv[1]
    victory = np.asarray(Image.open(os.path.join(folder, VICTORY)).convert("RGB"))
    icons = np.asarray(Image.open(os.path.join(folder, ICONS)).convert("RGB"))
    title = np.asarray(Image.open(os.path.join(folder, TITLE)).convert("RGB"))

    # Each entry: (C name, file name, pixels)
    pics = [
        ("story_picture", "01-story-picture.png",
         cover_sprite(victory, (17, 17, 910, 552), 480, 320)),
        ("jimmy_bob", "02-jimmy-bob.png",
         fit_sprite(victory, (933, 17, 1207, 301), 140, 140)),
        ("skeleton", "03-skeleton.png",
         fit_sprite(victory, (1226, 17, 1500, 302), 140, 140)),
        ("ghost", "04-ghost.png",
         fit_sprite(victory, (1519, 17, 1793, 302), 140, 140)),
        ("golden_eagle", "05-a-golden-eagle.png",
         fit_sprite(victory, (12, 582, 269, 852), 140, 140)),
        ("yellow_banana", "06-a-yellow-living-banana.png",
         fit_sprite(victory, (288, 582, 545, 852), 140, 140)),
        ("tasmanian_devil", "07-tasmanian-devil.png",
         fit_sprite(victory, (565, 582, 823, 852), 140, 140)),
        ("background", "08-background.png",
         cover_sprite(victory, (845, 582, 1695, 852), 480, 96, anchor="bottom")),
        ("platform", "09-platform.png",
         stretch_sprite(victory, (1708, 706, 1798, 756), 44, 13)),
        ("mega_boss", "10-mega-boss.png",
         fit_sprite(icons, (600, 21, 921, 335), 140, 140)),
        ("samurai_sword", "11-samurai-sword.png",
         fit_sprite(icons, (170, 387, 357, 618), 48, 48)),
        ("heart", "12-heart.png",
         fit_sprite(icons, (571, 392, 698, 503), 20, 20)),
        ("coin", "13-coin.png",
         fit_sprite(icons, (1168, 388, 1390, 615), 20, 20)),
        ("title_logo", "14-title-logo.png",
         cover_sprite(title, (0, 0, title.shape[1], title.shape[0]), 480, 231)),
        ("background_full", "17-background-full.png",
         background_dimmed(icons)),
        ("crossbow", "15-crossbow.png",
         fit_sprite(icons, (576, 638, 991, 981), 48, 48)),
        ("fire_bolt", "16-fire-bolt.png",
         fit_sprite(icons, (1108, 692, 1481, 914), 48, 48)),
    ]

    art_dir = os.path.join(ROOT, "art")
    os.makedirs(art_dir, exist_ok=True)
    h_lines = [
        "#ifndef NINJA_ART_H",
        "#define NINJA_ART_H",
        "",
        "#include <stdint.h>",
        "",
        "// Generated by tools/make_ninja_art.py: do not edit.",
        "// RGB565 pictures. 0xF81F (magenta, #FF00FF) is transparent in the sprites;",
        "// the story picture, background and title are opaque.",
        "#define NJ_KEY 0xF81F",
        "",
    ]
    c_lines = ['// Generated by tools/make_ninja_art.py: do not edit.', '#include "ninja_art.h"', ""]
    for name, fname, pix in pics:
        h, w = pix.shape[:2]
        Image.fromarray(pix).save(os.path.join(art_dir, fname))
        upper = name.upper()
        h_lines += [
            f"#define NJ_{upper}_W {w}",
            f"#define NJ_{upper}_H {h}",
            f"extern const uint16_t nj_{name}[NJ_{upper}_W * NJ_{upper}_H];",
            "",
        ]
        c_lines.append(f"const uint16_t nj_{name}[NJ_{upper}_W * NJ_{upper}_H] = {{")
        for y in range(h):
            c_lines.append("    " + ", ".join(f"0x{rgb565(pix[y, x]):04X}" for x in range(w)) + ",")
        c_lines += ["};", ""]
    h_lines += ["#endif", ""]
    with open(os.path.join(ROOT, "src", "ninja_art.h"), "w") as f:
        f.write("\n".join(h_lines))
    with open(os.path.join(ROOT, "src", "ninja_art.c"), "w") as f:
        f.write("\n".join(c_lines))
    print(f"wrote {len(pics)} pictures to art/ and src/ninja_art.c / .h")


if __name__ == "__main__":
    main()
