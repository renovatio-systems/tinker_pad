Here is the complete contents of your **`ASSETS.md`** file, ready to copy into Claude.

# Agent Jatayu — Level 3 Asset Guide

## Purpose

These ten PNG images support **Level 3: The Pursuit of Ravana**, a three-stage vertical platformer in the existing **Agent Jatayu** game. The game targets **RP2040 / RP2350** microcontrollers. Levels 1 and 2 should remain intact; Level 3 reuses the existing Level 2 Jatayu sprite.

The artwork uses **retro 16-bit-inspired pixel art**, strong dark outlines, and a limited warm palette inspired by Hindu temple architecture and Deepavali: saffron, gold, orange, deep red, navy blue, white, and dark brown. **None of the images contains game UI text**; the game draws its own text.

## Asset inventory

The source dimensions below were checked against the actual PNG files. The *game size* is the intended on-screen size, **not** the PNG's stored dimensions.

| File | Source dimensions | Intended game size | Role / placement |
| --- | ---: | ---: | --- |
| `story_ravana_kidnaps_sita.png` | 1536 × 1024 | 480 × 320 | Intro story scene: Ravana carries Sita away in the Pushpaka Vimana while Jatayu pursues them over temples and sea. |
| `pushpaka_vimana.png` | 1024 × 768 | 160 × 96 | Single combined chariot sprite containing Ravana and Sita; shown at the beginning and end of each stage. |
| `temple_platform.png` | 1024 × 1024 | 48 × 12 | Gold-trimmed stone ledge with marigold decorations; Stage 3A platforms. |
| `cloud_platform.png` | 1024 × 1024 | 48 × 14 | White cloud with blue underside and gold lotus ornaments; Stage 3B platforms. |
| `lotus_platform.png` | 1024 × 1024 | 40 × 12 | Floating lotus pad with pink-and-gold petals; decorative/alternate Stage 3B platform. |
| `dark_stone_platform.png` | 1024 × 1024 | 48 × 12 | Dark carved ledge with gold edging and red cracks; Stage 3C platforms. |
| `diya_lamp.png` | 1024 × 1024 | 12 × 16 | Small clay oil lamp with a flame; Stage 3A decorative accent. |
| `temple_silhouette_strip.png` | 1536 × 384 | 480 × 96 | Temple skyline strip with stepped towers, golden finials, and saffron-orange sunset; Stage 3A/3B background. |
| `cloud_tile.png` | 1024 × 1024 | 64 × 32 | White cloud against blue sky; Stage 3B background decoration. |
| `blood_moon.png` | 1024 × 1024 | 120 × 120 | Red cratered moon; static Stage 3C background focal point. |

## Stage-by-stage usage

### Stage 3A — Temple Ascent

- Background: a flat sunset colour, optionally with `temple_silhouette_strip.png` near the lower part of the scene.
- Platforms: `temple_platform.png`, used for both static and horizontally moving ledges.
- Decorations: small, sparingly placed `diya_lamp.png` sprites.
- Pursuit objective: `pushpaka_vimana.png`. Ravana and Sita are already part of this one sprite.

### Stage 3B — Celestial Sky

- Background: a flat light-blue sky with occasional `cloud_tile.png` decorations; a distant `temple_silhouette_strip.png` may be reused low in the scene.
- Platforms: `cloud_platform.png` and `lotus_platform.png`, supporting static, moving, and disappearing behaviours implemented in code.
- Pursuit objective: the same `pushpaka_vimana.png` sprite.

### Stage 3C — Blood Moon Pursuit

- Background: a flat dark red or navy sky with `blood_moon.png` positioned behind gameplay elements.
- Platforms: `dark_stone_platform.png`, reused for static, moving, and disappearing platforms.
- Pursuit objective: `pushpaka_vimana.png`; reaching it completes Level 3 and sets up the Ravana confrontation.

## Intro and transitions

- Show `story_ravana_kidnaps_sita.png` at the beginning of Level 3, scaled or cropped appropriately for the actual display.
- At the start of **each** of the three stages, show `pushpaka_vimana.png` briefly moving upward with Ravana and Sita aboard.
- When Jatayu reaches the stage goal, move the same combined sprite upward and transition to the next stage.
- At the end of Stage 3C, display the game's own completion text and proceed according to the existing level manager.
- **Do not bake any text into the images.**

## Transparency and image processing

- Most foreground sprites have an **RGB magenta (`#FF00FF`) colour-key background**. Treat exact `(255, 0, 255)` pixels as transparent when converting them for the game's renderer.
- The **story image**, **temple silhouette strip**, and **cloud tile** have their own coloured backgrounds and are **not** magenta-keyed.
- The supplied PNGs are **source artwork**. They are not already sized for the device and are not necessarily ready-to-flash assets.
- Crop excessive empty magenta margins around foreground sprites **before** scaling to their intended game dimensions. Preserve all visible details, such as chariot flags and the diya flame.
- Verify colour-key edges after conversion. Some edge pixels may be blended with magenta and need cleanup to avoid pink fringes.
- For the cloud tile, **verify seamless horizontal repetition before using it as a repeating tile**; do not assume the image edges match perfectly.
- The chariot's 1024 × 768 source canvas is not the same aspect ratio as its 160 × 96 game target. Crop to the visible chariot bounds and preserve its proportions rather than stretching the entire canvas.
- Use nearest-neighbour resampling for crisp pixel art. Keep visual size separate from collision/hitbox size.

## RP2040 / RP2350 integration notes

1. Inspect the existing display resolution, pixel format, rendering APIs, flash budget, and RAM usage first.
2. Convert the artwork **offline** into the existing engine's supported format (for example, palette-indexed sprites, RGB565 arrays, or whatever the project already uses). Do not decode full 1024-pixel PNGs at runtime unless the existing engine specifically supports that efficiently.
3. Store converted assets in flash where practical; avoid copying all assets into RAM.
4. Prefer a flat background colour plus a few small reused decorations rather than large layered bitmaps.
5. Reuse one chariot sprite for all three stages; no independent Ravana or Sita animation is required.
6. Reuse the existing Level 2 Jatayu sprite for standing and jumping. **No new Jatayu asset is included here.**
7. Use fixed-size platform arrays, integer or fixed-point movement, viewport culling, and the existing graphics/input/level systems.
8. **No audio:** do not add music, sound effects, audio libraries, or audio buffers.
9. Keep the existing game levels unchanged except for the minimum integration needed to enter Level 3.

## Suggested project layout

```text
assets/
  level3/
    ASSETS.md
    story_ravana_kidnaps_sita.png
    pushpaka_vimana.png
    temple_platform.png
    cloud_platform.png
    lotus_platform.png
    dark_stone_platform.png
    diya_lamp.png
    temple_silhouette_strip.png
    cloud_tile.png
    blood_moon.png
```

Adapt paths to the current repository rather than restructuring unrelated code.

## Instructions for Claude

> Inspect the existing Agent Jatayu codebase and these ten Level 3 PNGs. Convert them offline to the project's actual RP2040/RP2350 sprite format at the intended display sizes, taking care with magenta colour-keying, cropping, and aspect ratio. Integrate them into the three-stage vertical platformer: Temple Ascent, Celestial Sky, and Blood Moon Pursuit. Reuse Level 2's Jatayu sprite and the existing rendering, input, collision, and level-transition systems. The chariot is one combined sprite containing Ravana and Sita. Do not add audio, heavy animation, runtime PNG decoding, or new dependencies. Preserve Levels 1 and 2, and document any asset cleanup or compromises.

**Note:** The images are source artwork, so Claude should process and resize them before integrating them into the RP2040/RP2350 game.