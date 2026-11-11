# Agent Jatayu — Level 3 Finale, Story Scenes, and Ending

## Objective

After Level 3 is completed and the player continues, play five story scenes, one per picture, each with one line of narration. Then show a closing message and return to the title. There is **no Level 4 gameplay**: the story scenes are the whole of Level 4.

**Fidelity note:** In the *Ramayana*, Ravana mortally wounds Jatayu during the attempted rescue of Sita; **Rama and Lakshmana later find Jatayu**, who tells them what happened before dying. Do not show or imply that Jatayu runs to Rama.

## Assets

Source pictures are in `assets/level4/cutscenes/`. They are converted offline by `tools/make_l4_cutscenes.py` into `src/l4_cutscenes.c` and `src/l4_cutscenes.h` (480 × 320, RGB565, flash-resident).

| Scene | File | Event |
|---|---|---|
| 1 | `cut_1_abduction.png` | Ravana abducts Sita; Jatayu approaches. |
| 2 | `cut_2_battle.png` | Jatayu attacks Ravana and damages his weapons/chariot. |
| 3 | `cut_3_wings.png` | Ravana strikes Jatayu's wings. |
| 4 | `cut_4_fall.png` | Jatayu falls wounded; Ravana escapes with Sita. |
| 5 | `cut_5_rama.png` | Rama and Lakshmana find and comfort Jatayu. |

Do not regenerate, distort, or silently replace art unless requested. The game draws all text; do not embed captions in the PNGs.

## Flow

1. Level 3 ends with the existing **Continue** prompt (`SET: CONTINUE`). Nothing plays automatically.
2. When the player presses SET, scenes 1–5 play in order. A scene can be left with SET after 30 ticks (1 s) on screen.
3. Each scene shows its picture across the top and one narration line in a caption band at the bottom (`L4_CAP_Y`).
4. After scene 5, the closing message is shown. SET returns to the title after 40 ticks (about 1.3 s).

## One-line narration

One line per picture, with no paragraph text on screen. Wrap only if the font cannot fit the line at 480 px width.

| Scene | Narration |
|---|---|
| 1 — Abduction | Ravana carried Sita away, and brave Jatayu rose to stop him. |
| 2 — Battle | Jatayu fought Ravana fiercely and shattered his bow and chariot. |
| 3 — Wings | Ravana struck Jatayu's wings with his sword. |
| 4 — Fall | Wounded, Jatayu fell as Ravana fled with Sita. |
| 5 — Rama | Rama and Lakshmana found Jatayu, who revealed Sita's abduction. |

These are paraphrases, not quotations from a particular translation.

## Closing message

Shown after scene 5, in the game's text font, centred:

> JATAYU'S SACRIFICE WILL NEVER BE FORGOTTEN.
>
> Discover the timeless story of courage, devotion and dharma in the RAMAYANA, one of the world's oldest surviving epics.
>
> READ THE RAMAYANA. KEEP ITS WISDOM ALIVE.
>
> SHUBH DEEPAVALI!
>
> May the light of dharma always triumph over darkness.

Do not add a sequel teaser or another level.

## Hardware and implementation constraints

- Support **Raspberry Pi RP2040 and RP2350**.
- **No audio:** no music, sound effects, audio buffers, or audio libraries.
- Convert PNGs **offline**. Do not decode PNG at runtime or keep multiple full-screen bitmaps in RAM.
- Reuse the existing text routines and screen transitions. No new cutscene framework.
- Preserve Levels 1–3 and their assets, behaviour, and progression, except for the hook after Level 3 completion.

## Acceptance checklist

- [ ] Level 3 ends normally and waits for the player's SET press to continue.
- [ ] Scenes 1–5 display in that order, each with exactly one narration line.
- [ ] Scene 5 (Rama and Lakshmana) appears after scene 4, before the closing message.
- [ ] The closing message is shown, then SET returns to the title.
- [ ] The game never suggests that Jatayu literally ran to Rama.
- [ ] No audio, runtime PNG decoding, or unrelated level changes.
