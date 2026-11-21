// Wallet storage in flash. See wallet.h.
//
// Layout: a reserved area of two 4 KB sectors inside the firmware image, filled with 0xFF.
// Each sector holds 16 slots of 256 bytes (one flash page). A save appends a record to the
// next free slot, so a 4 KB erase happens only once per 16 saves. When the active sector
// is full, the other sector is erased and written, so the newest record is never lost to
// an erase or a power cut. Every record has a sequence number and a checksum; loading
// takes the valid record with the highest sequence number.

#include "wallet.h"

#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/flash.h"
#include "pico/stdlib.h"

#define SECTORS 2
#define SLOT_SIZE FLASH_PAGE_SIZE                      // 256
#define SLOTS_PER_SECTOR (FLASH_SECTOR_SIZE / SLOT_SIZE) // 16
#define MAGIC 0x31544C57u                              // "WLT1"

// The reserved area. It is initialised to 0xFF and is part of the .uf2, so flashing new
// firmware blanks it. `volatile` stops the compiler from folding reads into 0xFF.
// Aligned to a sector so erasing never touches the code or data next to it.
static const volatile uint8_t store[SECTORS * FLASH_SECTOR_SIZE]
    __attribute__((aligned(FLASH_SECTOR_SIZE), used)) = {[0 ... SECTORS * FLASH_SECTOR_SIZE - 1] = 0xFF};

typedef struct {
    uint32_t magic;
    uint32_t seq;
    wallet_t w;
    uint32_t sum;   // FNV-1a over everything above
} record_t;

_Static_assert(sizeof(record_t) <= SLOT_SIZE, "record must fit in one slot");

static uint32_t fnv1a(const void *p, size_t n) {
    const uint8_t *b = p;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 16777619u;
    return h;
}

static const volatile uint8_t *slot_ptr(int sector, int slot) {
    return store + sector * FLASH_SECTOR_SIZE + slot * SLOT_SIZE;
}

static bool slot_blank(int sector, int slot) {
    const volatile uint8_t *p = slot_ptr(sector, slot);
    for (int i = 0; i < SLOT_SIZE; i++)
        if (p[i] != 0xFF) return false;
    return true;
}

// Copies the slot out of flash and checks it.
static bool slot_read(int sector, int slot, record_t *r) {
    const volatile uint8_t *p = slot_ptr(sector, slot);
    for (size_t i = 0; i < sizeof *r; i++) ((uint8_t *)r)[i] = p[i];
    return r->magic == MAGIC && r->sum == fnv1a(r, offsetof(record_t, sum));
}

// Finds the newest valid record. Returns false if there is none.
static bool find_newest(int *sector, int *slot, record_t *out) {
    bool found = false;
    for (int s = 0; s < SECTORS; s++) {
        for (int i = 0; i < SLOTS_PER_SECTOR; i++) {
            record_t r;
            if (!slot_read(s, i, &r)) continue;
            if (!found || r.seq > out->seq) {
                found = true;
                *out = r;
                *sector = s;
                *slot = i;
            }
        }
    }
    return found;
}

bool wallet_load(wallet_t *w) {
    int s, i;
    record_t r;
    if (!find_newest(&s, &i, &r)) return false;
    *w = r.w;
    return true;
}

typedef struct {
    uint32_t sector_off;   // flash offset of the sector to erase, or UINT32_MAX for none
    uint32_t page_off;     // flash offset of the page to program
    uint8_t page[SLOT_SIZE];
} write_job_t;

// Runs with interrupts off and the other core parked (flash_safe_execute).
static void write_job(void *arg) {
    const write_job_t *j = arg;
    if (j->sector_off != UINT32_MAX) flash_range_erase(j->sector_off, FLASH_SECTOR_SIZE);
    flash_range_program(j->page_off, j->page, SLOT_SIZE);
}

bool wallet_save(const wallet_t *w) {
    int s = 0, i = -1;
    record_t cur;
    bool have = find_newest(&s, &i, &cur);

    record_t r = {.magic = MAGIC, .seq = have ? cur.seq + 1 : 1, .w = *w};
    r.sum = fnv1a(&r, offsetof(record_t, sum));

    // Pick the target: the next blank slot after the newest record in its sector,
    // otherwise slot 0 of the other (erased) sector.
    int ts, ti;
    bool erase = false;
    if (!have) {
        ts = 0;
        ti = 0;
        erase = !slot_blank(0, 0);   // leftovers from a torn write
    } else if (i + 1 < SLOTS_PER_SECTOR && slot_blank(s, i + 1)) {
        ts = s;
        ti = i + 1;
    } else {
        ts = (s + 1) % SECTORS;
        ti = 0;
        erase = true;
    }

    write_job_t j;
    memset(j.page, 0xFF, sizeof j.page);
    memcpy(j.page, &r, sizeof r);
    uint32_t sector_base = (uint32_t)(uintptr_t)store + ts * FLASH_SECTOR_SIZE - XIP_BASE;
    j.sector_off = erase ? sector_base : UINT32_MAX;
    j.page_off = sector_base + ti * SLOT_SIZE;

    if (flash_safe_execute(write_job, &j, 500) != PICO_OK) return false;

    // Read it back to be sure it stuck.
    wallet_t check;
    return wallet_load(&check) && memcmp(&check, w, sizeof check) == 0;
}
