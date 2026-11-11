# AGENT JATAYU

## LEVEL 3 — THE PURSUIT OF RAVANA

### RP2350-Optimised Vertical Platformer

## 1. Project Overview

Implement Level 3 of my existing Agent Jatayu game.

The game runs on a **Raspberry Pi RP2040 and RP2350 microcontroller**.

Levels 1 and 2 already exist.

**Do not modify or replace Levels 1 and 2 unless a small integration change is necessary.**

Level 3 is a simple 2D vertical platformer inspired by classic arcade games.

Jatayu must jump upward between platforms to pursue Ravana, who has kidnapped Sita aboard the Pushpaka Vimana.

There are three vertical stages within Level 3.

Each stage ends when Jatayu gets close to Ravana's flying chariot.

Ravana escapes upward, revealing the next stage.

### Hardware requirements

- Target: RP2350 microcontroller.
- No audio hardware is available.
- Do not implement music or sound effects.
- Use the existing display driver.
- Use the existing input controls.
- Use the existing graphics system.
- Avoid external dependencies.
- Keep memory consumption low.
- Prioritise smooth gameplay.

Before implementation, inspect the existing code to determine the display resolution, available RAM, programming language, graphics format, and input configuration.

Do not assume the game uses HTML Canvas, JavaScript, or a desktop game engine.

---

# 2. Gameplay

## Basic mechanics

Jatayu can:

1. Move left.
2. Move right.
3. Jump.
4. Land on platforms.
5. Ride moving platforms.
6. Avoid obstacles.
7. Reach Ravana's chariot.

Keep the controls simple.

### Recommended controls

Use the existing controls for:

- LEFT
- RIGHT
- JUMP

Do not introduce additional buttons unless absolutely necessary.

### Physics

Implement basic integer-based platformer physics.

Use:

- Horizontal velocity
- Vertical velocity
- Gravity
- Jump impulse
- Platform collision

Fixed-point arithmetic is preferred where appropriate.

Avoid floating-point calculations in the main game loop when integer or fixed-point calculations are sufficient.

### Jumping

Jatayu should be able to jump comfortably between platforms.

Use a consistent jump height.

Make the movement responsive rather than overly realistic.

Do not introduce complicated flying mechanics.

---

# 3. Three Vertical Stages

Level 3 contains three stages.

| Stage | Name               | Difficulty |
| ----- | ------------------ | ---------- |
| 3A    | Temple Ascent      | Easy       |
| 3B    | Celestial Sky      | Medium     |
| 3C    | Blood Moon Pursuit | Hard       |

Each stage should have a fixed, manageable height.

Use approximately 3–5 screen heights per stage, adjusted to the existing display resolution and available memory.

Do not create an infinite scrolling world.

---

# 4. Stage 3A — Temple Ascent

## Theme

Ancient Indian temples during Deepavali.

### Background

Use simple pixel-art graphics featuring:

- Orange sunset sky
- Temple silhouettes
- Small glowing diyas
- Golden decorations
- Marigold-inspired patterns

Keep the background mostly static.

Avoid particle effects and complex animations.

### Platforms

Two platform types:

**Static platforms**

Remain stationary.

**Moving platforms**

Move horizontally between two fixed positions.

Use simple back-and-forth movement.

### Difficulty

Easy.

Use wide platforms and short jumps.

### Objective

Jatayu climbs to the top of the temple.

Ravana and Sita are visible aboard the Pushpaka Vimana.

When Jatayu reaches the top, the chariot moves upward and disappears.

Display:

**RAVANA ESCAPES!**

Then transition to Stage 3B.

---

# 5. Stage 3B — Celestial Sky

## Theme

The sky above the temples.

### Background

Use:

- Light blue sky
- White clouds
- Distant temple silhouettes
- Small golden decorations
- Lotus-inspired platform patterns

Use a simple static or vertically scrolling background.

### Platforms

Three platform types:

**Static platforms**

Standard jumping platforms.

**Moving platforms**

Move horizontally.

**Disappearing platforms**

Disappear briefly after Jatayu lands.

Use a simple timer.

Avoid complicated animations.

### Difficulty

Medium.

Increase the spacing between platforms slightly.

### Objective

Jatayu climbs through the clouds.

At the top, he gets close to the Pushpaka Vimana.

Ravana escapes again.

Display:

**SO CLOSE!**

Transition to Stage 3C.

---

# 6. Stage 3C — Blood Moon Pursuit

## Theme

A dramatic crimson sky beneath a giant blood moon.

This stage should visually match the previously created Jatayu artwork.

### Background

Use:

- Dark red sky
- Large blood moon
- Dark cloud silhouettes
- Small golden temple decorations

The blood moon should be a static background graphic.

Avoid transparency effects, gradients calculated at runtime, and animated lighting.

### Platforms

Use:

- Static platforms
- Moving platforms
- Disappearing platforms

Make the moving platforms slightly faster.

Do not introduce new platform mechanics.

### Difficulty

Hard, but fair.

Every jump must be achievable.

### Final objective

Jatayu reaches the top of the stage.

The Pushpaka Vimana appears with Ravana and Sita.

Jatayu jumps toward the chariot.

Display:

**JATAYU CONFRONTS RAVANA!**

Then:

**LEVEL 3 COMPLETE**

This sets up the next level.

Do not implement the Ravana boss battle yet.

---

# 7. Ravana, Sita and Pushpaka Vimana

Keep these characters simple.

### Ravana

Use a small pixel-art sprite with:

- Golden crown
- Dark armour
- Red clothing

His ten-headed identity may be represented with a simplified multi-headed silhouette if the display resolution permits.

### Sita

Use a small pixel-art sprite with:

- Traditional sari
- Dark hair
- Gold accents

Portray her respectfully.

### Pushpaka Vimana

Use a single combined sprite showing:

- Golden flying chariot
- Ravana
- Sita

**Important optimisation:**

Do not animate Ravana, Sita, and the chariot separately unless necessary.

Use one combined sprite to reduce rendering overhead.

The chariot only needs two movement behaviours:

1. Hover.
2. Fly upward.

---

# 8. Jatayu Sprite

Reuse the existing Jatayu sprite.

Maintain:

- Gold armour
- White and brown feathers
- Navy-blue beak
- Diamond-blue eyes
- Wings
- Claws

For this level, Jatayu only needs:

- Standing pose
- Jumping pose

If only one sprite is available, reuse it for both states.

Do not create a complicated animation system.

### Sprite scaling

Scale Jatayu to fit the actual screen resolution.

Do not automatically use the earlier 240 × 240 display size if it is too large for the RP2350 display.

Use the existing game's sprite format and palette.

---

# 9. Vertical Scrolling Camera

Implement a basic vertical camera.

### Behaviour

The camera follows Jatayu upward.

When Jatayu jumps above a certain screen position, the world scrolls downward.

Use integer pixel coordinates.

Avoid complex camera easing.

### Suggested logic

```c
if (player_screen_y < CAMERA_THRESHOLD) {
    camera_y = player_world_y - CAMERA_THRESHOLD;
}
```

Adapt the coordinate signs to the existing engine.

The camera should never scroll outside the stage boundaries.

### Rendering

Only draw objects visible on the screen.

Do not render platforms that are far above or below the camera.

---

# 10. Platform Data

Use fixed arrays instead of dynamically allocating platforms.

Example:

```c
typedef struct {
    int16_t x;
    int16_t y;
    int16_t width;
    uint8_t type;
    int8_t speed;
    int16_t min_x;
    int16_t max_x;
    uint16_t timer;
} Platform;
```

Platform types:

```c
#define PLATFORM_STATIC 0
#define PLATFORM_MOVING 1
#define PLATFORM_DISAPPEARING 2
```

Suggested maximum:

**16–24 platforms per stage.**

Adjust this according to screen size and actual stage requirements.

Store stage layouts as constant data where possible.

Avoid heap allocation during gameplay.

---

# 11. Stage Transitions

Use a simple state machine.

```c
typedef enum {
    STAGE_3A,
    STAGE_3B,
    STAGE_3C,
    LEVEL_3_COMPLETE
} Level3Stage;
```

### Transition behaviour

At the start of each stage:

1. Show the Pushpaka Vimana with Ravana and Sita.
2. Move the chariot upward.
3. Begin gameplay.

At the end:

1. Detect Jatayu entering the goal area.
2. Pause player movement.
3. Move the chariot upward.
4. Display a short message.
5. Load the next stage.

Do not use video cutscenes.

Use simple sprite movement and text.

---

# 12. Deepavali Visual Style

Keep the Hindu and Deepavali aesthetic without overloading the RP2350.

### Colour palette

Use a limited palette:

| Colour     | Purpose                 |
| ---------- | ----------------------- |
| Saffron    | Temple sky              |
| Gold       | Chariot and decorations |
| Orange     | Diyas and sunset        |
| Deep red   | Blood moon              |
| Navy blue  | Jatayu                  |
| White      | Clouds                  |
| Dark brown | Temple structures       |

### Decorative details

Use small pixel-art elements:

- Diyas
- Temple arches
- Marigold garlands
- Lotus motifs
- Rangoli patterns

Prefer pre-rendered sprites or simple tile patterns.

Do not use procedural lighting or particle systems.

---

# 13. Graphics and Memory Optimisation

This game must run efficiently on the RP2350.

### Requirements

**Use static assets.**

Store graphics in flash memory wherever practical.

**Use a limited palette.**

Prefer indexed colours or the existing display format.

**Avoid large full-screen images.**

Use tiled backgrounds or small reusable sprites where possible.

**Avoid dynamic memory allocation.**

Use fixed-size arrays.

**Avoid unnecessary floating-point maths.**

Use integer or fixed-point movement.

**Only draw visible objects.**

Perform basic viewport culling.

**Reuse existing systems.**

Do not introduce a new rendering engine.

### Background rendering

Prefer:

1. Solid background colour.
2. Small repeating decorative tiles.
3. Static temple or cloud silhouettes.
4. Foreground platforms and sprites.

Avoid multiple parallax layers unless the existing engine already supports them efficiently.

### Frame rate

Target a stable frame rate appropriate to the existing display hardware.

30 FPS is a reasonable starting target if the display and driver can sustain it.

Prioritise consistent frame timing over higher frame rates.

---

# 14. Collision Detection

Use simple rectangular collision boxes.

For platform landing, check:

- Jatayu is falling.
- His feet crossed the platform top.
- His horizontal collision box overlaps the platform.

Only allow landing when approaching from above.

Moving platforms should carry Jatayu while he stands on them.

Do not implement pixel-perfect collision detection.

### Falling

If Jatayu falls below the visible play area, respawn him at the latest checkpoint.

Use one checkpoint approximately halfway through each stage.

Do not restart Levels 1 or 2.

---

# 15. User Interface

Keep the HUD minimal.

Display:

- Current stage
- Remaining lives or health, using the existing system
- Optional progress indicator

Do not introduce a complex interface.

### Example

```text
JATAYU          STAGE 3A
```

At the end of each stage:

```text
RAVANA ESCAPES!
```

At the end of Level 3:

```text
LEVEL 3 COMPLETE
```

Use the existing game's font rendering system.

---

# 16. No Audio

**The hardware has no audio output.**

Therefore:

- Do not implement background music.
- Do not implement sound effects.
- Do not include audio libraries.
- Do not allocate memory for audio buffers.
- Do not create audio tasks or interrupts.

Use visual feedback instead.

For example:

- Brief sprite flash when damaged.
- Small visual indicator when landing.
- Text message when a stage is completed.

Keep visual feedback simple.

---

# 17. Suggested Code Organisation

Adapt this to the existing project structure.

Suggested files:

```text
level3.h
level3.c
level3_platforms.h
level3_platforms.c
level3_assets.h
```

Responsibilities:

**level3.c**

- Level state machine
- Player updates
- Camera updates
- Stage transitions

**level3_platforms.c**

- Platform movement
- Platform collision
- Platform timers

**level3_assets.h**

- Sprite declarations
- Tile graphics
- Colour definitions

Avoid creating unnecessary files if the existing project already has suitable modules.

---

# 18. Implementation Order

Please implement this incrementally.

### Step 1 — Inspect existing code

Identify:

- RP2350 board configuration
- Display resolution
- Graphics library
- Input handling
- Existing player movement
- Existing level transitions
- Available memory
- Existing sprite assets

### Step 2 — Implement Stage 3A

Start with:

- Jatayu movement
- Jumping
- Static platforms
- Moving platforms
- Vertical camera
- Goal detection

Ensure this works before adding more features.

### Step 3 — Add Stage 3B

Reuse the same engine.

Change:

- Platform layout
- Background colours and tiles
- Platform speed
- Difficulty

Add disappearing platforms.

### Step 4 — Add Stage 3C

Reuse the same mechanics.

Change:

- Platform layout
- Background to blood moon
- Moving platform speed
- Difficulty

### Step 5 — Add chariot sequences

Implement:

- Opening chariot movement
- End-of-stage escape
- Simple transition messages

### Step 6 — Integrate

Connect Level 3 to the existing Level 2 completion logic.

Ensure Level 3 completion works with the game's existing level manager.

---

# 19. Testing

Test on the actual RP2350 hardware where available.

Check:

- No memory allocation failures.
- No crashes during stage transitions.
- Stable frame rate.
- Correct button response.
- Reliable platform collisions.
- Moving platforms carry Jatayu correctly.
- Disappearing platforms reset correctly.
- Camera scrolling works.
- No impossible jumps.
- No sprites appear outside expected boundaries.
- Levels 1 and 2 still function.
- Level 3 completes successfully.

If hardware is unavailable, distinguish between code-level testing and actual device testing.

---

# 20. Final Requirements

The finished game must have:

**Level 3A — Temple Ascent**

Jatayu jumps through a Deepavali-inspired temple environment.

**Level 3B — Celestial Sky**

Jatayu climbs through clouds and floating platforms.

**Level 3C — Blood Moon Pursuit**

Jatayu reaches Ravana's flying chariot beneath a crimson moon.

All three stages must:

- Run on the RP2350.
- Use simple 2D pixel-art graphics.
- Reuse existing game systems.
- Have no audio.
- Use minimal RAM.
- Have smooth vertical scrolling.
- Include moving platforms.
- Have reliable collision detection.
- Preserve Levels 1 and 2.

## IMPORTANT INSTRUCTION TO CLAUDE

This is an embedded microcontroller game, not a PC game.

**Do not overengineer it.**

Do not introduce:

- Game engines
- Physics libraries
- Audio systems
- Complex particle effects
- Large sprite animations
- Dynamic asset loading
- Unnecessary dependencies

Implement the smallest reliable solution that delivers the intended gameplay.

Prioritise:

1. Reliable operation on RP2350.
2. Responsive jumping.
3. Smooth platform movement.
4. Low memory usage.
5. Attractive but simple Deepavali graphics.
6. Clear three-stage progression.

**The goal is a polished, lightweight 16-bit-style vertical platformer that feels like a classic arcade game and runs smoothly on the RP2350.**
