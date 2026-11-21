You are building a small game for tinker_pad, a handheld board with a 480 × 320 colour screen
(16-bit RGB565), a five-direction joystick (UP, DOWN, LEFT, RIGHT, plus MID in the centre), and the
SET and RST buttons. RST resets the game. There is no sound and no operating system.
The firmware is written in C with the Raspberry Pi Pico SDK 2.x. The starter kit is the `release`
branch of the tinker_pad repo.

Only three functions in src/game.c need writing:
- game_init(): called once at power-up and again when RST is pressed.
- game_update(held, pressed): called 30 times a second. `held` lists the buttons down now;
  `pressed` lists the buttons that went down since the last tick.
- game_draw(buf, y0, rows): called once per strip of 8 rows. Draw into `buf` (RGB565 pixels).

Game name: Ninja Invasion

## Characters
Use these exact names in the code and text. Any other spelling of a name in the ideas below means one of these.
- Hero: Jimmy Bob — A lego looking ninja with a samurai sword in his back
- Villain: skeleton — equals 10 coins has a big sword
- Villain: ghost — The character has special phantom powers.
- Friend or creature: a golden eagle — shoots laser from its lasers
- Friend or creature: a yellow living banana — It  shoots banana peels.
- Friend or creature: Tasmanian devil — shoots yellow poo

## Ideas from the kids
Keep all content kind and suitable for children.

### Idea 1
- What would your game be about? Who is the hero? Jimmy Bob has friends - a golden eagle , a yellow living banana and a Tasmanian devil. Jimmy Bob defeats the mobs and the villains in the mob are skeleton and ghost. Jimmy Bob has these weapons  -a red dagger and a samurai sword made of dimond.
- What new enemy would you add? What does it look like, and how does it attack? Mega boss - a villain who shoots snake poison darts out of its mouth.
- What should a level be? What is the goal, and what makes it harder than the last one? Jimmy Bob fighting villains like skeleton and ghost . he is supported by his sidekicks - a golden eagle , a yellow living banana and Tasmanian devil. He can select between a red dagger and a samurai sword.
- What power would the hero get, and what does it cost? No magical powers
- What should the ending look like? Jimmy Bob defeating villains at every level to complete. And Jimmy Bob defeats Mega Boss to finish the game. He stands on top of fallen Mega Boss.

## Style picks from the kids
Use these where they fit the game.
- Favourite colours: Teal, Yellow, Black
- Design: Jungle and nature
- Feel: Ancient and royal

## Pictures
Finished PNGs are in the art/ folder, at the sizes shown.
- Story picture (Full-screen story picture): 480 × 320 px. file: art/01-story-picture.png.
- Jimmy Bob (Hero): 140 × 140 px. file: art/02-jimmy-bob.png.
- skeleton (Villain): 140 × 140 px. file: art/03-skeleton.png.
- ghost (Villain): 140 × 140 px. file: art/04-ghost.png.
- a golden eagle (Friend or creature): 140 × 140 px. file: art/05-a-golden-eagle.png.
- a yellow living banana (Friend or creature): 140 × 140 px. file: art/06-a-yellow-living-banana.png.
- Tasmanian devil (Friend or creature): 140 × 140 px. file: art/07-tasmanian-devil.png.
- Background (Wide background): 480 × 96 px. file: art/08-background.png.
- Platform (Platform to stand on): 44 × 13 px. file: art/09-platform.png.

## What to do
1. Propose a short plan: the controls, the goal, and how the ideas map to levels or features.
   Pick one simple first version, and say what you left out.
2. Implement it in src/game.c (and helper files if needed), using coloured rectangles where art is missing.
3. Keep to the screen size and a 30-tick update rate. Keep text large and simple.
4. Art: convert each PNG to a C array with the project's converter. Treat the background colour as
   transparent. For sprites use magenta #FF00FF, and document any other transparency colour you use.
5. Build with CMake 3.13 or newer, the Arm GCC toolchain and Pico SDK 2.x. The default board is
   pico2 (RP2350). For the RP2040 stick use -DPICO_BOARD=rp2040_usb_stick. The output is tinker_pad.uf2.
6. Explain what you built, how to build it, and what to test on the device.