/* SPDX-License-Identifier: GPL-3.0-only */
/* OMNI platform on the FM-1: plat.h over Felucca's HAL, panel map, USB rings and storage
 * (FoMni37: and on the SMK-37 Pro, over hal/smk_input.h).
 * Part of the unity build (omni.c), after the HAL, usb.c, storage.c and panel.c. */

uint32_t plat_ms(void) { return fm1_ms; }

uint32_t plat_buttons(void)
{
    uint32_t raw = fm1_in.buttons, out = 0, i;
    for (i = 0; i < NB; i++)
        if (raw & (1u << panel.btn[i]))
            out |= 1u << i;
    return out;
}

uint32_t plat_keys(void) { return fm1_in.notes & ((1u << NKEYS) - 1u); }

int32_t plat_enc(int role)
{
    if (role < 0 || role >= NE)
        return 0;
    return fm1_enc_take(panel.enc[role]) * panel.dir[role];
}

static volatile uint32_t master_q12 = 2048;
uint32_t plat_master(void) { return master_q12; }

#if BOARD_SMK37
/* ---- SMK-37 Pro (FoMni37): hal/smk_input.h. Button LEDs: one flag per button id (no dim level, so no glow);
 * the note keys have no LEDs: the pads show the strings (plat_pad_rgb, lcd.c padled_*) */
static void led_pos_init(void) {}
void plat_glow(int on) { (void)on; }
void plat_play_red(int red) { (void)red; }      /* SEQ PLAY has one LED */
static uint32_t smk_xled;
void plat_smk_leds(uint32_t lit) { smk_xled = lit; }
void plat_leds(uint32_t buttons, uint32_t keys)
{
    uint8_t nl[SMK_NLED] = {0};
    uint32_t i;
    (void)keys;
    for (i = 0; i < NB; i++)
        if ((buttons >> i) & 1u)
            nl[panel.btn[i]] = 1;
    for (i = 0; i < NSB; i++)                    /* KNOB BANK, FADER BANK, <, >: button ids 0..3 */
        if ((smk_xled >> i) & 1u)
            nl[i] = 1;
    for (i = 0; i < SMK_NLED; i++)
        fm1_led[i] = nl[i];
}

uint32_t plat_smk_buttons(void)
{
    return (fm1_in.buttons >> SMK_BTN_KNOB_BANK & 1u) << SB_KNOB_BANK |
           (fm1_in.buttons >> SMK_BTN_FADER_BANK & 1u) << SB_FADER_BANK |
           (fm1_in.buttons >> SMK_BTN_LEFT & 1u) << SB_LEFT | (fm1_in.buttons >> SMK_BTN_RIGHT & 1u) << SB_RIGHT;
}
uint32_t plat_note_hits(void) { return fm1_input_note_edges() & ((1u << NKEYS) - 1u); }
uint32_t plat_pad_hits(void) { return smk_pad_edges() & ((1u << NKEYS) - 1u); }
uint32_t plat_pad_notes(void) { return fm1_in.pnotes & ((1u << NKEYS) - 1u); }
uint64_t plat_keybed(void)
{
    uint32_t k = fm1__lock();                    /* (64 bits, written by the scan ISR) */
    uint64_t v = fm1_in.keys;
    fm1__unlock(k);
    return v;
}
uint64_t plat_keybed_hits(void) { return smk_keybed_edges(); }
int plat_key_vel(int k)
{
    uint32_t v = (uint32_t)k < (uint32_t)NKEYS ? fm1_in.key_vel[SMK_KEY_F3 + (uint32_t)k] : 0u;
    return v ? (int)v : 100;
}
int plat_fader(int i) { return fm1_in.frames < 200u || (uint32_t)i > 3u ? -1 : (int)fm1_in.fader[i]; }
void plat_pad_rgb(int string, uint8_t r, uint8_t g, uint8_t b)
{
    if ((uint32_t)string < SMK_NPAD)
        padled_set(SMK_PAD_OF_WHITE((uint32_t)string), r, g, b);
}

/* the wheels (mux C address 4 = pitch, 5 = mod), self-calibrating as in the reference port: the pitch wheel's
 * rest is its centre (taken once the readings settle, followed slowly while it rests), each wheel's range grows
 * to what it has reached (the travel is short: raw ~445..640 pitch, ~474..629 mod on a unit). Pitch: +-2
 * semitones with a dead band around the centre; mod: 0..127, up (lower raw) = more. */
#define SMK_BEND_ST 2
int plat_wheels(int32_t *bend_q12, int32_t *modw)
{
    enum { DEAD = 12, SPAN0 = 80, MOD_LO = 485, MOD_HI = 615 };
    static int32_t pc, plo, phi, mlo, mhi, rest, last_b = -1, last_m = -1;
    int32_t p = fm1_in.wheel[0], m = fm1_in.wheel[1], d, b, mw;
    if (fm1_in.frames < 300u)                    /* (the readings settle) */
        return 0;
    if (!pc) {
        pc = p;
        plo = p - SPAN0;
        phi = p + SPAN0;
        mlo = m < MOD_LO ? m : MOD_LO;
        mhi = m > MOD_HI ? m : MOD_HI;
    }
    if (p < plo)
        plo = p;
    if (p > phi)
        phi = p;
    if (m < mlo)
        mlo = m;
    if (m > mhi)
        mhi = m;
    d = p - pc;
    if (d > -DEAD && d < DEAD) {
        b = 0;
        if (++rest > 200) {                      /* resting: the centre follows a slow drift */
            pc += d > 0 ? 1 : d < 0 ? -1 : 0;
            rest = 0;
        }
    } else {
        rest = 0;
        b = d > 0 ? (d - DEAD) * (SMK_BEND_ST * 4096) / (phi - pc - DEAD > 1 ? phi - pc - DEAD : 1)
                  : (d + DEAD) * (SMK_BEND_ST * 4096) / (pc - plo - DEAD > 1 ? pc - plo - DEAD : 1);
        b = clamp(b, -SMK_BEND_ST * 4096, SMK_BEND_ST * 4096);
    }
    mw = mhi - m <= DEAD ? 0 : (mhi - m - DEAD) * 127 / (mhi - mlo - DEAD > 1 ? mhi - mlo - DEAD : 1);
    mw = clamp(mw, 0, 127);
    if (b == last_b && mw == last_m)
        return 0;
    last_b = b;
    last_m = mw;
    *bend_q12 = b;
    *modw = mw;
    return 1;
}

/* the factory velocity curves of the keys and pads: USRTRIM (flash 0xC1000, "trimdat" at +0x224), read only,
 * over the SPI read (no XIP window needed); smk_trim_load ignores a record that does not check out */
static uint8_t smk_trim_buf[0x22Cu] __attribute__((aligned(4)));
static void smk_trim_init(void)
{
    if (flash_ok && st_read(SMK_TRIM_FLASH, smk_trim_buf, sizeof smk_trim_buf) == 0)
        smk_trim_load(smk_trim_buf);
}
#else
/* LEDs: the picture is built off-line and copied one byte per column (Felucca: clearing
 * and relighting would let the 10 kHz scan catch the dark gap and flicker) */
static uint8_t led_pos[FM1_NKEY];
static void led_pos_init(void)
{
    uint32_t id, p, r;
    for (id = 0; id < FM1_NKEY; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)((p << 3) | r);
    }
}
/* Glow: every button not lit is lit dim, on one scan frame in GLOW_DIV (the HAL's fm1_led_dim) */
#define GLOW_DIV 4u
static uint8_t glow_on = 1;
void plat_glow(int on) { glow_on = (uint8_t)(on != 0); }
/* PLAY has two LEDs: its red at the button's own position, a green at the keyless matrix position
 * column 8, row PA5 (found on the device). Green, unless recording. */
static uint8_t play_red;
void plat_play_red(int red) { play_red = (uint8_t)(red != 0); }
void plat_leds(uint32_t buttons, uint32_t keys)
{
    uint8_t nl[FM1_NCOL] = {0}, dl[FM1_NCOL] = {0};
    uint32_t i, c;
    for (i = 0; i < NB + NKEYS; i++) {
        uint32_t id = i < NB ? panel.btn[i] : 14u + (i - NB);
        int on = i < NB ? (buttons >> i) & 1u : (keys >> (i - NB)) & 1u;
        uint8_t q = led_pos[id];
        if (i == B_PLAY && !play_red)            /* PLAY's green LED instead (keyless position col 8, row PA5) */
            q = (uint8_t)((8u << 3) | 1u);
        if (q == 0xFF)
            continue;
        if (on)
            nl[q >> 3] |= (uint8_t)(1u << (q & 7u));
        else if (i < NB && glow_on)
            dl[q >> 3] |= (uint8_t)(1u << (q & 7u));
    }
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led[c] = nl[c];
        fm1_led_dim[c] = dl[c];
    }
    fm1_led_dim_div = glow_on ? GLOW_DIV : 0u;
}

#endif

/* MIDI: Felucca's rings carry 4-byte USB-MIDI event packets, byte 0 = cable / CIN */
int plat_midi_in(uint32_t *pkt)
{
    if (mi_r == mi_w)
        return 0;
    *pkt = midi_in_q[mi_r % MQ];
    mi_r++;
    return 1;
}
void plat_midi_out(uint32_t pkt) { midi_out_event(pkt); }
/* the engine's MIDI out (from the render): a USB-MIDI packet, cable 0, CIN = the status's high nibble */
void om_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { plat_midi_out(st >> 4 | st << 8 | d1 << 16 | d2 << 24); }

int plat_store_load(uint32_t obj, void *dst, uint32_t max) { return flash_ok ? st_load(obj, dst, max) : -1; }
int plat_store_save(uint32_t obj, const void *src, uint32_t len) { return flash_ok ? st_save(obj, src, len) : -9; }

static volatile uint32_t audio_cpu_pct, audio_xruns;
uint32_t plat_cpu_pct(void) { return audio_cpu_pct; }
uint32_t plat_xruns(void) { return audio_xruns; }
