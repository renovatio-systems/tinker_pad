# Reckalambo, Level 1: design notes

## What the player does

A jaguar with a green-and-red hammer walks round a black-and-yellow Urus and wrecks it. Each
hit scores WRECK$ and does the same amount of damage to the car. When the car is wrecked it is
sold straight away, and the completion screen shows the result.

The Urus drives left and right across the screen and fires gorillas at the jaguar. The
hammer can knock a gorilla back at the car. The shield can block one. The third gorilla that
gets through restarts the level.

## Controls

| Control | Action |
|---|---|
| Joystick | Walk in 8 directions. The car is solid: walk round it |
| MID | Swing the hammer |
| SET | Raise the shield (during play). On the title, start the level. On the end screens, continue |
| RESET | Back to the title |

## WRECK$

There is one currency, WRECK$. Hits, knock-backs and the sale all add to it. The HUD shows
WRECK$ and HITS. There is no separate money.

## Scoring

Each hit, whether on the car or a gorilla, is worth 100 to 300 WRECK$:

- **Accuracy** is how close the hammer head is to the centre of its target. It gives up to
  120 points.
- **Speed** is how soon after the last hit this one landed. A hit within 3 seconds of the
  last one gets the speed bonus, which gives up to 80 points, falling to zero at 3 seconds.
- **Luck** adds a random amount from -20 to +20, then the result is kept between 100 and 300.

## The hammer

- A swing takes 12 ticks (0.4 s). The head is at the target on tick 6, which is the impact.
- The head hits the car if it is inside the car on tick 6. A hit scores and damages the car.
- On ticks 4 to 8 the head can also hit a flying gorilla. The gorilla is knocked back at the
  car, scoring WRECK$ for the hit. When it lands on the car it does that much damage.
- A swing that misses still counts as an attack, but scores nothing.
- Facing: the jaguar always faces the car. When the car is to his left, the sprite and the
  hammer are mirrored.

## The shield

- SET raises the shield for 12 ticks (0.4 s). It recharges for 90 ticks (3 s).
- While the shield is up, a gorilla that reaches the jaguar is blocked. It bounces back onto
  the car and does 100 damage to it. The jaguar takes no hit.
- The recharge bar is above his head, and the shield is outlined in cyan while it is up.

## The car

- The car drives left and right at 2 pixels per tick. It turns round at the edges of the
  screen, or when it would run into the jaguar.
- Health: 1500. The health bar is drawn above the car.
- At 0 health the car is wrecked and sold at once. The completion screen starts.

## The jaguar's movement

- He can walk anywhere on the play area: left, right, behind the car or in front of it.
- The car's bottom 20 pixels are solid, so he walks round it and cannot stand on it.
- He moves one axis at a time, so he slides along the car's edge instead of stopping dead.
- When he is behind the car, the car is drawn over him. In front, he is drawn over the car.

## Gorillas

- The car fires one gorilla every 2 seconds, starting after 2 seconds. Each flies at the
  jaguar's position at the moment it is fired. It lasts 80 ticks.
- A gorilla that reaches the jaguar counts as a hit, unless the shield blocks it.
- The jaguar takes 3 hits. The third hit restarts the level.

## Victory

The car is wrecked and sold at once. The sale is 500 WRECK$, plus a time bonus (10 WRECK$ for
each second under 90), plus a bounce bonus (150 WRECK$ for each gorilla bounced back).

The completion screen shows the jaguar doing a victory dance with the hammer up, the time, the
bounces, the sale and the total WRECK$. SET goes back to the title once it has been up for a
second.

## Not decided yet

- What WRECK$ buys: the shop, which cars, what they cost, and what "upgrade" means.
- Whether later cars differ in health, speed or firing rate.
- Whether the level has a time limit. Right now it does not.
- The final art for the gorillas and the car in the fight. The sprites are the ones
  generated so far.
