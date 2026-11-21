# Ninja Invasion — Armour Shop Integration

I'm attaching a PNG showing the final design for the **Armour Shop** in Ninja Invasion.

You already know the game's gameplay, hardware and existing code. Please integrate this screen without changing the existing gameplay.

## 1. Screen and controls

The handheld has a **480 × 320 colour display**.

Use the attached PNG as the visual reference. Scale or adapt it to fit the screen while keeping the artwork and text readable.

**Only these physical buttons work in this screen:**
- UP arrow
- DOWN arrow
- LEFT arrow
- RIGHT arrow
- MID (centre/select button)

There is no touchscreen interaction.

### Navigation

- **UP:** Move to the previous character row.
- **DOWN:** Move to the next character row.
- **LEFT:** Move to the previous armour tier.
- **RIGHT:** Move to the next armour tier.
- **MID:** Buy the selected armour, or equip it if already owned.

The selected armour must have a clearly visible highlight.

Do not allow the selection to move outside the four characters or five armour tiers.

### Back navigation

The back arrow in the top-left corner is a navigation item.

The player must be able to highlight it using the arrow buttons and activate it using MID.

When activated, return to the previous menu, or to the title screen if there is no previous menu.

The back arrow is not a touchscreen button.

## 2. Armour selection grid

There are four character rows:

1. Jimmy Bob
2. Banana
3. Golden Eagle
4. Tasmanian Devil

Each character has five armour tiers:

| Armour | Cost | DEF | LIVES |
|---|---:|---:|---:|
| Starter | 0 | 1 | 5 |
| Ninja | 100 | 3 | 6 |
| Royal | 400 | 8 | 10 |
| Legendary | 1,600 | 25 | 20 |
| Ultimate | 25,600 | 100 | 50 |

These are the current game-design values.

Use **Ultimate**, never Ultimate God.

Each character has its own armour artwork, as shown in the PNG.

The armour must remain visually distinguishable when rendered as a small gameplay sprite.

## 3. Dynamic fields in the PNG

The PNG contains blank fields that must be filled by the game at runtime.

Do not permanently draw the values into the background image.

### Wallet — top-right corner

The top-right field is labelled **Wallet:** and has a gold coin icon.

Display the player's current coin balance.

Example:

Wallet: 2,500

The number must update immediately after earning or spending coins.

The wallet is shared across all four characters.

### Selected — bottom-left panel

The bottom-left panel contains the label **Selected:** and an empty rounded rectangle.

Display the currently highlighted character and armour tier.

Examples:

- Jimmy Bob — Ninja
- Banana — Royal
- Golden Eagle — Legendary
- Tasmanian Devil — Ultimate

Update this field whenever the player changes the selection.

### DEF — bottom-centre panel

Beside the blue shield icon is **DEF:** with an empty field.

Display the defence value of the highlighted armour.

### LIVES — bottom-centre panel

Beside the red heart icon is **LIVES:** with an empty field.

Display the lives value of the highlighted armour.

DEF and LIVES must update whenever the selected armour changes.

### Cost — bottom-right panel

Beside the gold coin icon is **Cost:** with an empty rounded rectangle.

Display the price of the highlighted armour.

Examples:

- Starter: 0
- Ninja: 100
- Royal: 400
- Legendary: 1,600
- Ultimate: 25,600

If the armour is already purchased, display **OWNED** instead of its price.

If it is currently equipped, display **EQUIPPED**.

## 4. Buy button

The green **BUY** button is located at the bottom-right corner.

This is a visual indicator of the action performed by MID.

When the selected armour is available for purchase:

- Show BUY.
- Pressing MID purchases it.
- Subtract the price from Wallet.
- Save the purchase.
- Equip the armour.

When already owned but not equipped:

- Show EQUIP.
- Pressing MID equips it without charging again.

When currently equipped:

- Show EQUIPPED.
- Pressing MID must not charge any coins.

When the player cannot afford it:

- Show NOT ENOUGH COINS.
- Do not deduct any coins.
- Do not unlock the armour.

Starter armour is free and available from the beginning.

## 5. Character armour and gameplay

Each character has its own equipment state.

For example, Jimmy Bob can have Royal armour while Banana has Ninja armour.

Buying or equipping armour for one character must not change another character's equipment.

When an armour is equipped, update that character's gameplay sprite and the relevant DEF and LIVES statistics.

Use the corresponding armour artwork for each character and tier.

Preserve the existing gameplay mechanics unless a change is required for the armour system.

## 6. Rendering requirements

The game runs on a small 3.5-inch, 480 × 320 display.

The supplied PNG is the visual reference, but it may be too detailed to display directly at native resolution.

Please:

- Keep the same overall visual style and colour coding.
- Preserve the four characters and five armour tiers.
- Use bold, readable text.
- Render dynamic values over the correct blank fields.
- Keep the selection highlight clearly visible.
- Use nearest-neighbour scaling for pixel-art sprites.
- Avoid tiny text and unnecessary animation.
- Do not stretch the artwork out of proportion.

If the full four-row grid becomes unreadable at 480 × 320, adapt it into a compact scrolling interface while retaining the same controls, fields and shop behaviour.

## 7. Saving and restoring

Save the following information using the game's existing save system:

- Current wallet balance
- Purchased armour for each character
- Equipped armour for each character

Do not charge the player twice for an already purchased item.

When the player returns to the shop, restore the saved state.

## 8. Implementation

Please integrate this into the existing Ninja Invasion project.

Reuse the existing button-reading, rendering, currency and save systems.

Do not invent new hardware button mappings if the project already defines them.

Deliver the necessary code changes and explain which files were modified.

The final result should look like the supplied PNG, but be fully functional on the handheld using **only the four directional arrows and MID button**.