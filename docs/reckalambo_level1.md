# Reckalambo, Level 1: design notes

## What the player does

A jaguar with a green-and-red hammer walks around the screen and wrecks a black-and-yellow Lamborghini Urus. The more damage a swing does, the more points it earns. Once the car is wrecked, the player sells it. The sale gives money, which is for buying and upgrading later.

The Urus fires gorillas at the jaguar. If the jaguar is hit 11 times, the level restarts.

## Controls

| Control | Action |
|---|---|
| Joystick | Walk in 8 directions |
| MID | Swing the hammer |
| SET | Sell the wreck (when it is wrecked). On the title, start the level. On the end screens, continue |
| RESET | Back to the title |

## Hammer and points

- A swing takes 12 ticks (0.4 s). The head is at its lowest point on tick 6, which is the impact.
- A swing only scores if the head is inside the car at impact. Otherwise it is a miss, which still counts as an attack.
- Points = 100 + 200 × quality. Quality is 1 when the head is at the centre of the car and 0 at the edge of the hit zone.
- Each hit takes the same number of health points from the car as the points it scores.

## The Urus

- Health: 1500. The health bar is drawn above the car.
- At 0 health the car is wrecked. It turns grey and smoke rises from it. Gorillas stop.
- The sale bonus is 500 on top of the points. Money = points + 500.

## Gorillas

- The car fires one gorilla every 2 seconds (60 ticks), starting after 2 seconds.
- Each gorilla flies in a straight line at the jaguar's position when it fires. It lasts 80 ticks, which is 2.7 s.
- A gorilla that hits the jaguar counts as a hit. The jaguar then has 20 ticks of no damage, so one gorilla counts once.
- The 11th hit restarts the level.

## Victory

1. The car is wrecked. Press SET to sell it. The sale shows the money earned.
2. The jaguar does a victory dance for 4 seconds.
3. The level-complete screen shows points and money. SET goes back to the title (the shop comes later).

## Not decided yet

- Whether the money is spent on any cars in Level 1 or saved for the shop.
- The shop: which cars, what they cost, and what "upgrade" means.
- Whether later cars have more health or fire faster.
- Whether the level has a time limit. Right now it does not.
- The final art for the jaguar, the Urus and the gorilla. The placeholder shapes are just to test the game.
