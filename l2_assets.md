# Agent Jatayu — Sprite Assets Integration Guide

## 1. Overview

I have generated a collection of retro 16-bit pixel-art character sprites for my game, **Agent Jatayu**.

The sprites are supplied in a ZIP archive:

`agent_jatayu_all_sprite_options.zip`

The goal is to replace the game's existing character graphics with these new sprites while preserving the existing gameplay, animations, movement, collision detection, and combat mechanics.

**Please inspect the ZIP archive and existing game code before making changes.**

---

## 2. Characters

The game contains four fighters.

| Character | Filename | Target display size |
|---|---|---|
| Jatayu | `jatayu.png` | 240 × 240 px |
| Shadow Spearman | `shadow_spearman.png` | 108 × 132 px |
| Blood Warrior | `blood_warrior.png` | 108 × 132 px |
| Demon Commander | `demon_commander.png` | 132 × 156 px |

These are the new target display dimensions, three times the previous character sizes.

### Jatayu

The main playable character.

- Noble giant vulture warrior inspired by the Ramayana.
- Golden armour.
- Powerful wings.
- Clawed fists.
- Fighting stance facing right.
- Retro arcade fighting-game appearance.

### Shadow Spearman

Enemy with the Shadow Step ability.

- Dark purple and near-black body.
- Glowing eyes.
- Shadow effects.
- Carries a spear.

### Blood Warrior

Enemy with the Blood Rage ability.

- Muscular Rakshasa demon.
- Crimson and dark-red colouring.
- Curved horns.
- Clawed hands.

### Demon Commander

Enemy with the Thunder Roar ability.

- Large armoured Rakshasa commander.
- Dark green skin.
- Heavy armour and crown.
- Lightning effects around the fists.

---

## 3. Understanding the ZIP Contents

The archive contains different ways of accessing the character artwork.

### A. Individual Jatayu variations

These are alternative designs of the main playable character.

They allow us to choose which version of Jatayu looks best in the game.

Only one Jatayu design needs to be used during gameplay.

The remaining designs can be kept as alternatives.

### B. Sprite sheet

The sprite sheet contains all four fighters in one image.

The layout is:

| Top left | Top right |
|---|---|
| Jatayu | Shadow Spearman |
| Blood Warrior | Demon Commander |

A sprite sheet is useful because multiple characters can be stored in a single image.

Game engines can extract individual sprites by selecting rectangular regions of the sheet.

However, this particular sheet contains **one pose per character**.

It is not an animation sheet.

### C. `sheet_characters/` folder

This folder contains the characters extracted individually from the combined sprite sheet.

Expected contents:

```text
sheet_characters/
├── jatayu.png
├── shadow_spearman.png
├── blood_warrior.png
└── demon_commander.png
```

These characters share a consistent visual style because they originate from the same generated image.

**This is the recommended starting point for integrating all four fighters.**

The extracted images may contain unused magenta space, so their visible character bounds should be checked.

---

## 4. Sprite Sheet vs Individual PNGs

| Feature | Sprite sheet | Individual PNGs |
|---|---|---|
| Contains all fighters | Yes | Separate files |
| Easy to replace one fighter | Less convenient