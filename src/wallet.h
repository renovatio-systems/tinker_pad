#ifndef WALLET_H
#define WALLET_H

// The wallet: coins and bought armour that survive power-off and RST.
//
// It lives in flash, inside the firmware image. Flashing a new .uf2 rewrites that
// area as blank, so the wallet is erased exactly when new firmware is flashed
// (and not by power cycles, RST or the 1200-baud reboot into boot mode).

#include <stdbool.h>
#include <stdint.h>

#define WALLET_ITEMS 4  // one armour set per character

typedef struct {
    int32_t coins;
    uint16_t owned[WALLET_ITEMS];     // bit i set: armour i is bought
    uint8_t equipped[WALLET_ITEMS];   // the armour each character wears
} wallet_t;

// Reads the newest saved wallet. Returns false (and leaves *w alone) if none is saved.
bool wallet_load(wallet_t *w);

// Saves the wallet. Stalls the CPU for a few milliseconds (up to ~50 ms when a flash
// sector has to be erased), so call it between fights. Returns true on success.
bool wallet_save(const wallet_t *w);

#endif
