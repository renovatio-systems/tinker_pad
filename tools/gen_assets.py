#!/usr/bin/env python3
"""Builds src/assets.c and src/assets.h: the Level 1 artwork as RGB565 arrays.

    python3 -I tools/gen_assets.py <jatayu.png> [preview.png]

Jatayu is cut out of the title artwork (jatayu.png). The Rakshasas and the forest
background are drawn here. Sprites use KEY (magenta) as the transparent colour.
"""
import math
import os
import random
import sys

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter
from scipy import ndimage as ndi

KEY = 0xF81F
S = 4  # supersampling for drawn sprites

# Outline of Jatayu in jatayu.png (1536 x 1024), traced by hand clockwise from the wing tip.
BIRD_OUTLINE = [
    (30, 630), (80, 430), (160, 290), (250, 200), (360, 115), (480, 60), (560, 40), (620, 35),
    (670, 60), (685, 110), (670, 170), (640, 230), (645, 290), (700, 310), (715, 230), (710, 185),
    (745, 115), (840, 88), (905, 105), (935, 130), (975, 160), (1000, 185), (998, 225), (950, 242),
    (940, 290), (985, 320), (1030, 320), (1100, 330), (1135, 400), (1150, 490), (1142, 580),
    (1112, 700), (1092, 780), (1075, 835), (1100, 870), (1145, 905), (1120, 915), (1080, 895),
    (1000, 880), (940, 875), (900, 850), (890, 865), (800, 865), (700, 855), (690, 830), (600, 840),
    (500, 880), (440, 890), (400, 880), (360, 870), (300, 900), (240, 945), (205, 920), (200, 860),
    (165, 800), (135, 850), (110, 790), (60, 700),
]
# Talon areas of the artwork, which get a golden glow.
CLAW_BOXES = [(670, 790, 910, 890), (910, 830, 1150, 925)]

GOLD = (255, 200, 60)


def c565(r, g, b):
    v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return v + 1 if v == KEY else v


def sprite_words(img, flat_key=True):
    """RGBA image -> list of RGB565, transparent pixels = KEY."""
    a = np.asarray(img.convert("RGBA")).astype(int)
    out = []
    for y in range(a.shape[0]):
        for x in range(a.shape[1]):
            r, g, b, al = a[y, x]
            out.append(KEY if al < 128 else c565(r, g, b))
    return out


def jatayu_cutout(path):
    im = Image.open(path).convert("RGB")
    m = Image.new("L", im.size, 0)
    ImageDraw.Draw(m).polygon(BIRD_OUTLINE, fill=255)
    mask = np.asarray(m) > 0
    a = np.asarray(im).astype(int)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    mx = np.maximum(g, b)
    mask &= ~(r > 1.9 * mx + 40)  # leftover red sky inside the outline
    yy, xx = np.mgrid[0:a.shape[0], 0:a.shape[1]]
    mask &= ~((xx < 560) & (yy > 560) & (r > 1.45 * mx + 10))  # red scraps around the tail
    mask = ndi.binary_opening(mask, iterations=1)
    lab, n = ndi.label(mask)
    sizes = ndi.sum(mask, lab, range(1, n + 1))
    mask = lab == (1 + int(np.argmax(sizes)))

    # The title is lit red: take the red out of the feathers, then lift the whites so the
    # head and chest read as bright cream against navy-black wings.
    a = a.astype(float)
    a[..., 0] *= 0.82
    a[..., 1] = a[..., 1] * 1.12 + 4
    a[..., 2] = a[..., 2] * 1.30 + 8
    a = 255.0 * (np.clip(a, 0, 255) / 255.0) ** 0.78
    a = np.clip(a * 1.08, 0, 255)
    lum = (a[..., 0] + a[..., 1] + a[..., 2]) / 3
    # Golden magical claws: the dark blue talon pixels turn gold.
    for x0, y0, x1, y1 in CLAW_BOXES:
        sel = np.zeros(mask.shape, bool)
        sel[y0:y1, x0:x1] = True
        sel &= mask & (lum < 110) & (a[..., 2] > a[..., 0] + 6)
        a[sel] = GOLD
    rgba = np.dstack([a.astype("uint8"), (mask * 255).astype("uint8")])
    return Image.fromarray(rgba, "RGBA")


# ----- Jatayu poses ----------------------------------------------------------------------

# The big raised wing, and the shoulder it swings around, in jatayu.png coordinates.
WING_POLY = [(0, 0), (720, 0), (720, 320), (600, 520), (330, 720), (0, 720)]
SHOULDER = (700, 320)
OUTLINE = (12, 6, 22, 255)


def jatayu_wing_frames(full):
    """Two full-size frames from the artwork: wings raised and wings swept down."""
    wing_mask = Image.new("L", full.size, 0)
    ImageDraw.Draw(wing_mask).polygon(WING_POLY, fill=255)
    alpha = full.getchannel("A")
    body = full.copy()
    body.putalpha(ImageChops.multiply(alpha, ImageChops.invert(wing_mask)))
    wing = full.copy()
    wing.putalpha(ImageChops.multiply(alpha, wing_mask))
    # something dark behind the shoulder, so the swung wing leaves no hole
    under = Image.new("RGBA", full.size, (0, 0, 0, 0))
    ImageDraw.Draw(under).polygon([(560, 260), (760, 260), (760, 520), (600, 600), (520, 480)], fill=(26, 30, 64, 255))
    under.putalpha(ImageChops.multiply(under.getchannel("A"), alpha.filter(ImageFilter.MaxFilter(15))))
    frames = []
    for angle in (-4, 30):  # degrees counter-clockwise: wings up, wings down
        w = wing.rotate(angle, center=SHOULDER, resample=Image.BICUBIC)
        comp = Image.alpha_composite(under, w)
        frames.append(Image.alpha_composite(comp, body))
    return frames


def finish_sprite(im):
    """Sharpen, then give the bird a dark outline so it stands out against the forest."""
    rgb = im.convert("RGB").filter(ImageFilter.UnsharpMask(radius=1.0, percent=150, threshold=2))
    arr = np.asarray(rgb).copy()
    solid = np.asarray(im.getchannel("A")) > 110
    ring = ndi.binary_dilation(solid, iterations=1) & ~solid
    out = np.zeros(arr.shape[:2] + (4,), dtype="uint8")
    out[solid] = np.dstack([arr, np.full(arr.shape[:2], 255, "uint8")])[solid]
    out[ring] = OUTLINE
    return Image.fromarray(out, "RGBA")


def jatayu_poses(path):
    """[flap][lean]: lean 0 = flying left, 1 = level, 2 = flying right. Every pose is cut from
    one big canvas around the same point, so flapping and leaning don't make him jump."""
    full = jatayu_cutout(path)
    x0, y0, x1, y1 = full.getbbox()
    cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
    poses = []
    for frame in jatayu_wing_frames(full):
        big = Image.new("RGBA", (1800, 1800), (0, 0, 0, 0))
        big.paste(frame, (900 - cx, 900 - cy))
        row = []
        for angle in (-4, -12, -20):  # leaning forward, more so the faster he flies right
            r = big.rotate(angle, resample=Image.BICUBIC).crop((300, 300, 1500, 1500))
            row.append(finish_sprite(r.resize((80, 80), Image.LANCZOS)))
        poses.append(row)
    return poses


# ----- Rakshasas -------------------------------------------------------------------------

def P(pts):
    return [(x * S, y * S) for x, y in pts]


def glow(canvas, box, colour, radius, alpha):
    layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(layer).ellipse([v * S for v in box], fill=colour + (alpha,))
    layer = layer.filter(ImageFilter.GaussianBlur(radius * S))
    return Image.alpha_composite(canvas, layer)


def rakshasa(w, h, skin, dark, armor, trim, eye, horn, aura, big=False, spear=False,
             crown=False, marks=False):
    canvas = Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))
    cx = w / 2
    sc = 1.22 if big else 1.0
    canvas = glow(canvas, (cx - 17 * sc, h * 0.25, cx + 17 * sc, h * 0.95), aura, 3, 110)
    d = ImageDraw.Draw(canvas)

    # spear behind the body
    if spear:
        d.line(P([(cx + 12, 1), (cx + 12, h - 3)]), fill=(70, 50, 40, 255), width=int(1.5 * S))
        d.polygon(P([(cx + 12, -1), (cx + 9.5, 6), (cx + 12, 4.5), (cx + 14.5, 6)]),
                  fill=(210, 150, 255, 255))
        canvas = glow(canvas, (cx + 8, 0, cx + 16, 9), (200, 120, 255), 1.5, 160)
        d = ImageDraw.Draw(canvas)

    # levitating lower body: a tapering wisp instead of legs
    d.polygon(P([(cx - 7 * sc, h * 0.66), (cx + 7 * sc, h * 0.66), (cx + 4, h * 0.88),
                 (cx + 1, h), (cx - 1, h * 0.93), (cx - 4, h * 0.86)]), fill=dark + (235,))
    d.polygon(P([(cx - 5 * sc, h * 0.72), (cx + 5 * sc, h * 0.72), (cx, h * 0.97)]),
              fill=aura + (120,))

    # arms
    d.polygon(P([(cx - 9 * sc, h * 0.42), (cx - 15 * sc, h * 0.50), (cx - 17 * sc, h * 0.70),
                 (cx - 13 * sc, h * 0.71), (cx - 11 * sc, h * 0.55), (cx - 7 * sc, h * 0.50)]),
              fill=skin + (255,))
    d.polygon(P([(cx + 9 * sc, h * 0.42), (cx + 15 * sc, h * 0.50), (cx + 17 * sc, h * 0.70),
                 (cx + 13 * sc, h * 0.71), (cx + 11 * sc, h * 0.55), (cx + 7 * sc, h * 0.50)]),
              fill=skin + (255,))
    for sx in (-1, 1):  # clawed fists
        d.ellipse([v * S for v in (cx + sx * 15 * sc - 2.2, h * 0.68, cx + sx * 15 * sc + 2.2, h * 0.76)],
                  fill=dark + (255,))

    # torso and armour
    d.polygon(P([(cx - 9 * sc, h * 0.38), (cx + 9 * sc, h * 0.38), (cx + 7 * sc, h * 0.68),
                 (cx - 7 * sc, h * 0.68)]), fill=armor + (255,))
    d.polygon(P([(cx - 6 * sc, h * 0.42), (cx + 6 * sc, h * 0.42), (cx + 4.5 * sc, h * 0.60),
                 (cx - 4.5 * sc, h * 0.60)]), fill=skin + (255,))
    d.rectangle([v * S for v in (cx - 7.5 * sc, h * 0.62, cx + 7.5 * sc, h * 0.67)], fill=trim + (255,))
    if marks:
        for k in (-3, 0, 3):
            d.line(P([(cx + k * sc, h * 0.44), (cx + k * 0.6 * sc, h * 0.58)]), fill=eye + (255,), width=S)
    # pauldrons
    for sx in (-1, 1):
        d.ellipse([v * S for v in (cx + sx * 10 * sc - 4 * sc, h * 0.34, cx + sx * 10 * sc + 4 * sc, h * 0.45)],
                  fill=trim + (255,))
        d.ellipse([v * S for v in (cx + sx * 10 * sc - 2 * sc, h * 0.355, cx + sx * 10 * sc + 2 * sc, h * 0.405)],
                  fill=armor + (255,))

    # horns
    for sx in (-1, 1):
        d.polygon(P([(cx + sx * 5 * sc, h * 0.17), (cx + sx * 9 * sc, h * 0.12),
                     (cx + sx * 11 * sc, h * 0.01), (cx + sx * 7.5 * sc, h * 0.09),
                     (cx + sx * 4 * sc, h * 0.10)]), fill=horn + (255,))
    # head
    d.ellipse([v * S for v in (cx - 6.5 * sc, h * 0.09, cx + 6.5 * sc, h * 0.30)], fill=skin + (255,))
    d.polygon(P([(cx - 6 * sc, h * 0.22), (cx + 6 * sc, h * 0.22), (cx + 3 * sc, h * 0.34),
                 (cx - 3 * sc, h * 0.34)]), fill=skin + (255,))
    if crown:
        for k in (-4, -2, 0, 2, 4):
            top = h * (0.01 if k == 0 else 0.05)
            d.polygon(P([(cx + (k - 1) * sc, h * 0.12), (cx + k * sc, top), (cx + (k + 1) * sc, h * 0.12)]),
                      fill=(230, 170, 40, 255))
        d.rectangle([v * S for v in (cx - 5.5 * sc, h * 0.11, cx + 5.5 * sc, h * 0.14)], fill=(230, 170, 40, 255))
    # brow, eyes, fangs
    for sx in (-1, 1):
        d.line(P([(cx + sx * 1.2 * sc, h * 0.175), (cx + sx * 5 * sc, h * 0.145)]), fill=dark + (255,), width=S)
    for sx in (-1, 1):
        canvas = glow(canvas, (cx + sx * 3 * sc - 2, h * 0.16, cx + sx * 3 * sc + 2, h * 0.24), eye, 1.2, 230)
        d = ImageDraw.Draw(canvas)
        d.ellipse([v * S for v in (cx + sx * 3 * sc - 1.3, h * 0.185, cx + sx * 3 * sc + 1.3, h * 0.225)],
                  fill=(255, 255, 220, 255))
    d.rectangle([v * S for v in (cx - 3.5 * sc, h * 0.265, cx + 3.5 * sc, h * 0.29)], fill=(40, 0, 10, 255))
    for sx in (-1, 1):
        d.polygon(P([(cx + sx * 2.8 * sc - 0.9, h * 0.265), (cx + sx * 2.8 * sc + 0.9, h * 0.265),
                     (cx + sx * 2.8 * sc, h * 0.325)]), fill=(250, 250, 240, 255))
    return canvas.resize((w, h), Image.LANCZOS)


def blood_warrior():
    return rakshasa(36, 44, skin=(150, 22, 28), dark=(60, 8, 14), armor=(95, 62, 34),
                    trim=(170, 120, 50), eye=(255, 230, 40), horn=(220, 205, 170), aura=(120, 10, 20))


def shadow_spearman():
    return rakshasa(36, 44, skin=(88, 34, 124), dark=(34, 14, 56), armor=(48, 30, 72),
                    trim=(120, 80, 170), eye=(255, 40, 40), horn=(150, 140, 175), aura=(90, 30, 150),
                    spear=True)


def demon_commander():
    return rakshasa(44, 52, skin=(70, 18, 28), dark=(20, 6, 12), armor=(28, 22, 30),
                    trim=(150, 20, 30), eye=(255, 90, 20), horn=(40, 30, 40), aura=(150, 20, 10),
                    big=True, crown=True, marks=True)


# ----- Background ------------------------------------------------------------------------

def background(w=480, h=320, seed=7):
    rnd = random.Random(seed)
    np.random.seed(seed)
    y = np.arange(h)[:, None].astype(float)
    stops = [(0, (30, 4, 10)), (120, (84, 10, 18)), (220, (150, 32, 24)), (262, (190, 70, 34)), (h, (40, 8, 14))]
    img = np.zeros((h, w, 3))
    for c in range(3):
        img[..., c] = np.interp(y, [s[0] for s in stops], [s[1][c] for s in stops]) * np.ones((1, w))
    im = Image.fromarray(img.astype("uint8"), "RGB").convert("RGBA")

    # storm clouds: dark blurred blobs plus a few lit edges
    clouds = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    cd = ImageDraw.Draw(clouds)
    for _ in range(46):
        cx, cy = rnd.randint(-20, w + 20), rnd.randint(10, 200)
        rx, ry = rnd.randint(30, 90), rnd.randint(6, 16)
        cd.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=(18, 2, 10, rnd.randint(70, 130)))
    clouds = clouds.filter(ImageFilter.GaussianBlur(5))
    im = Image.alpha_composite(im, clouds)
    lit = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ld = ImageDraw.Draw(lit)
    for _ in range(18):
        cx, cy = rnd.randint(0, w), rnd.randint(120, 230)
        rx = rnd.randint(20, 70)
        ld.ellipse([cx - rx, cy - 3, cx + rx, cy + 3], fill=(220, 80, 50, 70))
    im = Image.alpha_composite(im, lit.filter(ImageFilter.GaussianBlur(3)))

    # a dim blood moon behind the ruins
    moon = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(moon).ellipse([60, 38, 120, 98], fill=(230, 80, 60, 150))
    im = Image.alpha_composite(im, moon.filter(ImageFilter.GaussianBlur(2)))

    d = ImageDraw.Draw(im)

    def ridge(base, amp, colour, seed2):
        r2 = random.Random(seed2)
        pts, x = [(0, h)], 0
        while x <= w + 20:
            pts.append((x, base - r2.randint(0, amp)))
            x += r2.randint(14, 40)
        pts += [(w, h)]
        d.polygon(pts, fill=colour)

    ridge(238, 46, (70, 14, 38, 255), 1)   # distant mountains
    ridge(262, 30, (44, 8, 30, 255), 2)

    # temple ruins on the right: stepped tower (shikhara), columns, a broken arch
    ruin = (26, 7, 22, 255)
    tx = 345
    for i, (hw, top) in enumerate([(34, 236), (28, 214), (23, 196), (18, 182), (13, 170), (8, 160)]):
        d.rectangle([tx - hw, top, tx + hw, 280], fill=ruin)
    d.polygon([(tx - 4, 160), (tx, 144), (tx + 4, 160)], fill=ruin)
    d.rectangle([tx - 1, 134, tx + 1, 146], fill=ruin)
    for cx0, top in [(285, 232), (299, 224), (396, 226), (410, 238), (424, 218)]:
        d.rectangle([cx0 - 4, top, cx0 + 4, 282], fill=ruin)
        d.rectangle([cx0 - 6, top - 3, cx0 + 6, top], fill=ruin)
    d.arc([400, 196, 446, 250], 180, 340, fill=ruin, width=7)
    d.rectangle([270, 270, 440, 284], fill=ruin)
    # a few dim glowing windows
    for wx, wy in [(345, 226), (345, 204), (345, 188)]:
        d.rectangle([wx - 2, wy, wx + 2, wy + 7], fill=(190, 80, 30, 255))

    # forest silhouettes along the bottom and the sides
    tree = (8, 2, 10, 255)
    for x in range(-10, w + 20, 11):
        top = 262 + rnd.randint(-10, 18)
        d.rectangle([x, top + 14, x + 3, h], fill=tree)
        d.ellipse([x - 14, top - 6, x + 17, top + 28], fill=tree)
    for x0 in (14, 458):
        d.rectangle([x0 - 4, 70, x0 + 4, h], fill=tree)
        for k in range(7):
            ty = 80 + k * 26
            d.polygon([(x0, ty - 12), (x0 - 24 + (k % 2) * 6, ty + 16), (x0 + 24 - (k % 2) * 6, ty + 16)], fill=tree)
    for x0 in (60, 420):
        d.rectangle([x0 - 3, 150, x0 + 3, h], fill=tree)
        for k in range(5):
            ty = 160 + k * 24
            d.polygon([(x0, ty - 10), (x0 - 18, ty + 14), (x0 + 18, ty + 14)], fill=tree)
    d.rectangle([0, 304, w, h], fill=(6, 1, 8, 255))

    return im.convert("RGB")


# ----- output ----------------------------------------------------------------------------

def emit_array(f, name, w, h, words):
    f.write(f"const uint16_t {name}[{w * h}] = {{\n")
    for i in range(0, len(words), 12):
        f.write("    " + ", ".join(f"0x{v:04X}" for v in words[i:i + 12]) + ",\n")
    f.write("};\n\n")


def main():
    src = sys.argv[1]
    preview = sys.argv[2] if len(sys.argv) > 2 else None
    here = os.path.dirname(os.path.abspath(__file__))
    srcdir = os.path.join(here, "..", "src")

    poses = jatayu_poses(src)
    icon = poses[0][1].resize((16, 16), Image.LANCZOS)
    bg = background()
    r1, r2, r3 = blood_warrior(), shadow_spearman(), demon_commander()

    sprites = [(f"spr_jatayu_f{f}_l{l}", poses[f][l]) for f in (0, 1) for l in (0, 1, 2)]
    sprites += [
        ("spr_jatayu_icon", icon), ("spr_blood_warrior", r1), ("spr_shadow_spearman", r2),
        ("spr_demon_commander", r3),
    ]

    with open(os.path.join(srcdir, "assets.h"), "w") as f:
        f.write("#ifndef ASSETS_H\n#define ASSETS_H\n\n#include <stdint.h>\n\n")
        f.write("// Generated by tools/gen_assets.py: do not edit.\n")
        f.write(f"#define SPRITE_KEY 0x{KEY:04X} // transparent pixels\n\n")
        f.write(f"#define BG_W {bg.width}\n#define BG_H {bg.height}\n")
        f.write("extern const uint16_t bg_forest[BG_W * BG_H];\n\n")
        for name, im in sprites:
            u = name.upper()
            f.write(f"#define {u}_W {im.width}\n#define {u}_H {im.height}\n")
            f.write(f"extern const uint16_t {name}[{u}_W * {u}_H];\n")
        f.write("\n#endif\n")

    with open(os.path.join(srcdir, "assets.c"), "w") as f:
        f.write('#include "assets.h"\n\n// Generated by tools/gen_assets.py: do not edit.\n\n')
        bgw = [c565(*px) for px in bg.get_flattened_data()]
        emit_array(f, "bg_forest", bg.width, bg.height, bgw)
        for name, im in sprites:
            emit_array(f, name, im.width, im.height, sprite_words(im))

    if preview:
        sheet = Image.new("RGB", (3000, 320 * 2 + 300), (40, 90, 40))
        sheet.paste(bg.resize((960, 640), Image.NEAREST), (0, 0))
        x = 8
        for name, im in sprites:
            big = im.resize((im.width * 4, im.height * 4), Image.NEAREST)
            sheet.paste(big, (x, 650), big)
            x += big.width + 8
        sheet.save(preview)


if __name__ == "__main__":
    main()
