#!/usr/bin/env python3
"""Convert shop.png (the Armour Shop design, 1536 x 1024) into in-game pictures.

Usage: python3 tools/make_shop_art.py shop.png

Writes src/shop_art.c and src/shop_art.h:
  shop_bg                    the whole shop screen at 480 x 320 (RGB565), opaque. The parts the game
                             draws itself are blanked: the wallet value, the Selected / DEF / LIVES /
                             Cost fields, the BUY text, the preview picture and the highlight that the
                             design shows around Jimmy Bob's Ultimate.
  shop_sprite[kind][tier]    the armoured characters as 140 x 140 sprites for use in play, cut out of the
                             picture. Magenta 0xF81F is transparent. Tier 0 (Starter) is NULL: the game
                             keeps using its original art for that.
kind: 0 Jimmy Bob, 1 Banana, 2 Golden Eagle, 3 Tasmanian Devil. tier: 0 Starter .. 4 Ultimate.

The cut-out is approximate: sprites sit on glowing coloured rows, so each one keeps a little of its
glow, which looks like an aura in play.
"""
import os
import sys

import numpy as np
from PIL import Image
from scipy import ndimage as ndi

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
KEY = (255, 0, 255)
KEY565 = 0xF81F

# Where the sprites are in shop.png: the y band of each character row (above the name pills)
# and the x centre of each of the five tier columns.
ROWS = [(100, 216), (283, 396), (458, 566), (636, 746)]
COLS = [525, 743, 960, 1180, 1420]
HALF = 108


def rgb565(a):
    a = a.astype(np.uint16)
    v = ((a[..., 0] & 0xF8) << 8) | ((a[..., 1] & 0xFC) << 3) | (a[..., 2] >> 3)
    return v


def cut_cell(im, r, c, thr=70, trim=2):
    """Returns the cell pixels and a mask of the sprite in it."""
    y0, y1 = ROWS[r]
    x = COLS[c]
    band = im[y0:y1, 430:1530]
    bg = np.median(band, axis=1)                     # the row's background colour at each y
    cell = im[y0:y1, x - HALF:x + HALF].copy()
    d = np.abs(cell - bg[:, None, :]).sum(axis=2)
    if (r, c) == (0, 4):
        return cell, jimmy_ultimate_mask(cell, d)
    m = d > thr
    m = ndi.binary_opening(m, iterations=2)
    lab, n = ndi.label(m)
    sizes = ndi.sum(m, lab, range(1, n + 1))
    m = lab == (1 + int(np.argmax(sizes)))
    m = ndi.binary_fill_holes(ndi.binary_closing(m, iterations=2))
    if trim:
        m = ndi.binary_erosion(m, iterations=trim)
    return cell, m


def jimmy_ultimate_mask(cell, d):
    """Jimmy Bob's Ultimate sits in a gold highlight box. Use his dark outline instead."""
    dark = cell.sum(axis=2) < 230
    inner = np.zeros_like(dark)
    inner[8:, 14:-10] = True
    dark &= inner
    m = ndi.binary_fill_holes(ndi.binary_closing(dark, iterations=3))
    m = ndi.binary_opening(m, iterations=2)
    lab, n = ndi.label(m)
    sizes = ndi.sum(m, lab, range(1, n + 1))
    m = lab == (1 + int(np.argmax(sizes)))
    ext = ndi.binary_dilation(m, iterations=20) & (d > 140) & inner   # the crown has no dark outline
    ext = ndi.binary_opening(ext, iterations=1)
    return ndi.binary_fill_holes(ndi.binary_closing(ext | m, iterations=3))


def to_sprite(cell, m):
    """Fits the masked sprite into 140 x 140 (feet on the bottom edge), magenta elsewhere."""
    ys, xs = np.where(m)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    c = cell[y0:y1, x0:x1].astype(np.uint8)
    mm = m[y0:y1, x0:x1]
    idx = ndi.distance_transform_edt(~mm, return_distances=False, return_indices=True)
    c = c[idx[0], idx[1]]        # colours under the mask edge come from the sprite, not the background
    h, w = mm.shape
    s = min(140 / w, 140 / h)
    nw, nh = max(1, round(w * s)), max(1, round(h * s))
    ca = np.array(Image.fromarray(c).resize((nw, nh), Image.LANCZOS))
    ma = np.array(Image.fromarray((mm * 255).astype(np.uint8)).resize((nw, nh), Image.LANCZOS)) > 127
    out = np.empty((140, 140, 3), np.uint8)
    out[:] = KEY
    ox, oy = (140 - nw) // 2, 140 - nh
    sub = out[oy:oy + nh, ox:ox + nw]
    sub[ma] = ca[ma]
    return out


def clean_background(im):
    """The screen with everything the game draws itself blanked out."""
    bg = im.copy()

    # 1. Jimmy Bob's Ultimate is shown highlighted in the design. Repaint that cell with the row
    #    colour, keeping his name pill and himself.
    x0, x1, y0, y1 = 1296, 1530, 90, 272
    cell, m = cut_cell(im, 0, 4)
    row = np.median(im[y0:y1, 440:1290], axis=1)
    bg[y0:y1, x0:x1] = row[:, None, :]
    bg[212:262, 1330:1510] = im[212:262, 1330:1510]
    cy0 = ROWS[0][0]
    cx0 = COLS[4] - HALF
    region = bg[cy0:cy0 + m.shape[0], cx0:cx0 + m.shape[1]]
    region[m] = cell[m]

    # 2. The preview picture at the bottom left: the game draws the selected armour there.
    px0, px1, py0, py1 = 26, 246, 824, 994
    col = np.median(im[py0:py1, 248:256], axis=1)
    bg[py0:py1, px0:px1] = col[:, None, :]

    # 3. The word "Buy": the game writes BUY, EQUIP or EQUIPPED. Fill the button with its own colour.
    bx0, bx1, by0, by1 = 1290, 1450, 858, 950
    left = im[by0:by1, 1262:1274].mean(axis=1)
    right = im[by0:by1, 1470:1480].mean(axis=1)
    t = np.linspace(0, 1, bx1 - bx0)[None, :, None]
    bg[by0:by1, bx0:bx1] = (left[:, None, :] * (1 - t) + right[:, None, :] * t)
    return bg


def write_array(f, name, values):
    f.write(f"const uint16_t {name}[{len(values)}] = {{\n")
    for i in range(0, len(values), 16):
        f.write("    " + ",".join(f"0x{v:04X}" for v in values[i:i + 16]) + ",\n")
    f.write("};\n\n")


def flat565(a, key_ok):
    v = rgb565(a).reshape(-1)
    if not key_ok:   # an opaque picture must never contain the transparent colour
        v[v == KEY565] = KEY565 ^ 0x0020
    else:            # in a sprite only the magenta background may be the transparent colour
        isbg = (a.reshape(-1, 3) == np.array(KEY)).all(axis=1)
        v[(v == KEY565) & ~isbg] = KEY565 ^ 0x0020
    return [int(x) for x in v]


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "shop.png")
    im = np.array(Image.open(src).convert("RGB")).astype(int)
    assert im.shape[:2] == (1024, 1536), "shop.png should be 1536 x 1024"

    bg = clean_background(im).astype(np.uint8)
    bg480 = np.array(Image.fromarray(bg).resize((480, 320), Image.LANCZOS))

    sprites = {}
    for r in range(4):
        for c in range(1, 5):
            cell, m = cut_cell(im, r, c)
            sprites[(r, c)] = to_sprite(cell, m)

    out_c = os.path.join(ROOT, "src", "shop_art.c")
    out_h = os.path.join(ROOT, "src", "shop_art.h")
    with open(out_c, "w") as f:
        f.write("// Generated by tools/make_shop_art.py from shop.png: do not edit.\n")
        f.write('#include "shop_art.h"\n\n')
        write_array(f, "shop_bg", flat565(bg480, False))
        for (r, c), s in sprites.items():
            write_array(f, f"shop_sprite_{r}_{c}", flat565(s, True))
        f.write("const uint16_t *const shop_sprite[4][5] = {\n")
        for r in range(4):
            f.write("    {0, " + ", ".join(f"shop_sprite_{r}_{c}" for c in range(1, 5)) + "},\n")
        f.write("};\n")
    with open(out_h, "w") as f:
        f.write("""#ifndef SHOP_ART_H
#define SHOP_ART_H

#include <stdint.h>

// Generated by tools/make_shop_art.py from shop.png: do not edit.
#define SHOP_BG_W 480
#define SHOP_BG_H 320
#define SHOP_SPRITE_W 140
#define SHOP_SPRITE_H 140

// The Armour Shop screen with its changing parts left blank. Opaque.
extern const uint16_t shop_bg[SHOP_BG_W * SHOP_BG_H];

// Armoured characters: [0 Jimmy Bob, 1 Banana, 2 Golden Eagle, 3 Tasmanian Devil][tier 0-4].
// Magenta 0xF81F is transparent. Tier 0 (Starter) is NULL: use the game's original art.
extern const uint16_t *const shop_sprite[4][5];

#endif
""")
    print("wrote", out_c, out_h)
    # a preview for a quick look
    prev = Image.fromarray(bg480)
    prev.save(os.path.join(os.path.dirname(out_c), "..", "art", "shop-background.png")) if False else None


if __name__ == "__main__":
    main()
