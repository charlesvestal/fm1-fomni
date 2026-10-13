/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37 Pro flash map (build with -DBOARD_SMK37=1):
 * every 4 KiB sector of the 1 MiB part through the app's allow-list (FL_RANGE_OK / FL_GUARD, fm1_flash_map.h)
 * and the update loader's (loader.c): none of them may ever reach the head 0x0-0x3FFF, BTIF / USRTRIM
 * 0xC0000-0xC1FFF, USR 0xF4000-0xFDFFF or key_mac 0xFF000-; the store's objects (storage.c) and the OTA
 * staging (ota.c) lie inside the allowed windows; ranges that wrap or straddle a boundary are refused.
 * Then the store owner tag (storage.c st_owner_check) on a simulated flash: ours kept, another family's
 * or untagged data cleared (only the object area), the tag written. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fm1_cc.h"
#if !BOARD_SMK37
#error "build with -DBOARD_SMK37=1"
#endif

#include "fm1_flash_map.h"
#include "../firmware/src/app/plat.h"      /* OBJ_* */

static uint8_t nor[0x100000];
static uint32_t bad_write;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    if (!FL_GUARD(off, 0x1000u) || (off & 0xFFFu)) {
        bad_write++;
        return -8;
    }
    memset(nor + off, 0xFF, 0x1000u);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (!FL_GUARD(off, n) || ((off & 0xFFu) + n) > 256u) {
        bad_write++;
        return -8;
    }
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
#include "../firmware/src/storage.c"
#define LDR_RANGE_OK(off, n) FL_LDR_RANGE_OK(off, n)       /* what loader.c uses on the SMK */

static int bad;
static void check(const char *what, int ok) { printf("smk flash: %-78s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }

static int protected_sector(uint32_t s)
{
    return s < 0x4000u || (s >= 0xC0000u && s < 0xC2000u) || (s >= 0xF4000u && s < 0xFE000u) || s >= 0xFF000u;
}

static int all(uint32_t lo, uint32_t hi, uint8_t v)
{
    uint32_t i;
    for (i = lo; i < hi; i++)
        if (nor[i] != v)
            return 0;
    return 1;
}

int main(void)
{
    uint32_t s, o, c, leak_app = 0, leak_ldr = 0, leak_guard = 0, allowed = 0, in_store = 1;
    for (s = 0; s < 0x100000u; s += 0x1000u) {
        if (!protected_sector(s))
            continue;
        leak_app += FL_RANGE_OK(s, 0x1000u) || FL_STORE_OK(s, 0x1000u) || FL_RANGE_OK(s, 256u) || FL_RANGE_OK(s + 0xF00u, 256u);
        leak_ldr += LDR_RANGE_OK(s, 0x1000u) || LDR_RANGE_OK(s + 0xF00u, 256u);
        leak_guard += !FL_PROTECTED(s, 0x1000u) || !FL_PROTECTED(s + 0xF00u, 256u);
    }
    for (s = 0; s < 0x100000u; s += 0x1000u)
        allowed += FL_GUARD(s, 0x1000u);
    check("no protected sector passes the app's allow-list (erase, page)", leak_app == 0);
    check("no protected sector passes the loader's allow-list", leak_ldr == 0);
    check("the RAM driver's own net (FL_PROTECTED) covers every protected sector", leak_guard == 0);
    check("straddling ranges refused: 0xBF000+0x2000, 0x3000+0x2000",
          !FL_GUARD(0xBF000u, 0x2000u) && !LDR_RANGE_OK(0xBF000u, 0x2000u) &&
          !LDR_RANGE_OK(0x3000u, 0x2000u) && FL_PROTECTED(0xBF000u, 0x2000u) && FL_PROTECTED(0x3F00u, 0x200u));
    check("wrapping ranges refused (off + n past 4 GiB)",
          !FL_GUARD(0xBB000u, 0xFFFFF000u) && !LDR_RANGE_OK(0x9D000u, 0xFFFFFF00u) && FL_PROTECTED(0xBB000u, 0xFFFFF000u));
    check("app windows: FL_DATA 0x9D000-0xBAFFF, FL_OTA 0xBB000-0xBFFFF, nothing else (no sample slots)",
          FL_GUARD(0x9D000u, 0x1000u) && FL_GUARD(0xBA000u, 0x1000u) && FL_GUARD(0xBF000u, 0x1000u) &&
          !FL_GUARD(0xC2000u, 0x1000u) && !FL_GUARD(0x9C000u, 0x1000u) &&
          allowed == (0xC0000u - 0x9D000u) / 0x1000u);
    check("loader: app area 0x4000-0x9CFFF, records 0x9D000-0xBFFFF only",
          LDR_RANGE_OK(0x4000u, 0x1000u) && LDR_RANGE_OK(0x9C000u, 0x1000u) && LDR_RANGE_OK(0xBF000u, 0x1000u) &&
          !LDR_RANGE_OK(0xC0000u, 0x1000u) && FL_REC_HI == 0xC0000u);
    for (o = 0; o < OBJ_NOBJ; o++)
        for (c = 0; c < 2u; c++)
            in_store &= FL_STORE_OK(st_sector(o, c), 0x1000u) && st_sector(o, c) < ST_OWNER_OFF;
    {   /* every object's two copies distinct */
        uint32_t o2, c2, distinct = 1;
        for (o = 0; o < OBJ_NOBJ; o++)
            for (c = 0; c < 2u; c++)
                for (o2 = 0; o2 < OBJ_NOBJ; o2++)
                    for (c2 = 0; c2 < 2u; c2++)
                        if ((o != o2 || c != c2) && st_sector(o, c) == st_sector(o2, c2))
                            distinct = 0;
        check("storage.c: every object (A/B) in FL_DATA below the owner tag, no two share a sector", in_store && distinct);
    }
    check("storage.c: project 0x9D000/0x9E000, panel 0x9F000/0xA0000, owner tag 0xAF000",
          st_sector(OBJ_PROJ, 0) == 0x9D000u && st_sector(OBJ_PROJ, 1) == 0x9E000u &&
          st_sector(OBJ_PANEL, 0) == 0x9F000u && st_sector(OBJ_PANEL, 1) == 0xA0000u && ST_OWNER_OFF == 0xAF000u);
    check("FL_PLAIN_LO = the VM start 0x9D000", FL_PLAIN_LO == 0x9D000u);
    check("FL_JEDEC_OK: the SMK part 0xEB6014 and 0x856014 only", FL_JEDEC_OK(0xEB6014u) && FL_JEDEC_OK(0x856014u) &&
          !FL_JEDEC_OK(0u) && !FL_JEDEC_OK(0xFFFFFFu) && !FL_JEDEC_OK(0xEB6015u));

    /* the store owner (FELUCCA_DATA_FAMILY "FOMNI", FELUCCA_DATA_UNTAGGED_OK 0) */
    {
        static const uint8_t TAG[16] = "SMK37DATAFOMNI";
        char a[300], got[300];
        int n;
        memset(nor, 0xA5, sizeof nor);           /* everything outside the store: must stay */
        memset(nor + 0x9D000u, 0xFF, 0xBB000u - 0x9D000u);
        memset(a, 'A', sizeof a);
        check("FELUCCA_DATA_UNTAGGED_OK defaults to 0, the family to FOMNI",
              FELUCCA_DATA_UNTAGGED_OK == 0 && !strcmp(FELUCCA_DATA_FAMILY, "FOMNI"));
        /* 1. untagged data (another firmware from before the tag, or stock VM data): cleared, tag written */
        memset(nor + 0x9D000u, 0x11, 0x1000u);
        memset(nor + 0xAE000u, 0x22, 0x1000u);
        st_owner_check();
        check("untagged store: object area cleared, tag FOMNI written",
              st_owner_cleared && all(0x9D000u, 0xAF000u, 0xFF) && !memcmp(nor + 0xAF000u, TAG, 16));
        /* 2. ours: kept */
        st_owner_cleared = 0;
        check("save a project", st_save(OBJ_PROJ, a, sizeof a) == 0);
        st_owner_check();
        n = st_load(OBJ_PROJ, got, sizeof got);
        check("our tag: nothing cleared, the project loads",
              !st_owner_cleared && n == (int)sizeof a && !memcmp(got, a, sizeof a));
        /* 3. another family's tag: cleared, taken over */
        memset(nor + 0xAF000u, 0xFF, 0x1000u);
        memcpy(nor + 0xAF000u, "SMK37DATASLOOP", 15);
        st_owner_check();
        check("another family (SLOOP): object area cleared, tag FOMNI written",
              st_owner_cleared && st_load(OBJ_PROJ, got, sizeof got) < 0 && all(0x9D000u, 0xAF000u, 0xFF) &&
              !memcmp(nor + 0xAF000u, TAG, 16));
        check("nothing outside 0x9D000..0xAFFFF touched, no write refused",
              all(0, 0x9D000u, 0xA5) && all(0xB0000u, 0xBB000u, 0xFF) && all(0xBB000u, 0x100000u, 0xA5) &&
              bad_write == 0);
    }
    printf("%s\n", bad ? "SMK FLASH TEST FAILED" : "smk flash test passed");
    return bad != 0;
}
