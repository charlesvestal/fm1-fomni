/* SPDX-License-Identifier: GPL-3.0-only */
/* OMNI platform interface (from X0X's): everything the app needs from the hardware.
 *
 * The device implementation (plat_fm1.c) wraps Felucca's HAL and panel map; the
 * host implementation (host/omni_host.c) is a simulator fed by a script, which is
 * what lets the whole app — UI, sequencer, engines — run and be tested on a Mac.
 * Drawing goes through Felucca's lcd_fill / lcd_blit (lcd.c on the device, a
 * framebuffer on the host) and gfx.c on top. */
#pragma once
#include <stdint.h>

/* big zero-initialised buffers go in the .pool RAM region on the device */
#ifdef OM_HOST
#define OM_POOL
#else
#define OM_POOL __attribute__((section(".pool")))
#endif

/* panel controls, by printed label (Felucca's panel.c map turns these into matrix ids; Felucca
 * calls the second button SCL, the panel prints SEL) */
enum { B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC,
       B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };

#define NKEYS 27                       /* F3 .. G5 */

uint32_t plat_ms(void);
uint32_t plat_buttons(void);           /* held: bit B_* */
uint32_t plat_keys(void);              /* held: bit k = key k (0 = F3) */
int32_t plat_enc(int role);            /* detents since the last call, + = clockwise; EN_* */
uint32_t plat_master(void);            /* MASTER pot, 0..4096 gain Q12 */
void plat_leds(uint32_t buttons, uint32_t keys);   /* lit: bit B_*, bit key */
void plat_glow(int on);                /* the unlit buttons glow dimly, readable in the dark (call before plat_leds) */
void plat_play_red(int red);           /* PLAY lights green; red (its other LED) while recording (before plat_leds) */

/* USB MIDI, 4-byte USB-MIDI event packets (cable 0) */
int plat_midi_in(uint32_t *pkt);       /* 1 = got one (called from the audio ISR) */
void plat_midi_out(uint32_t pkt);      /* queue one (audio ISR or main loop) */

/* persistent storage (Felucca storage.c objects: A/B sectors, CRC, torn-write safe) */
int plat_store_load(uint32_t obj, void *dst, uint32_t max);   /* bytes loaded, < 0 = none / bad */
int plat_store_save(uint32_t obj, const void *src, uint32_t len);   /* 0 = ok */
#define PLAT_STORE_MAX 3840u           /* bytes per object */
enum { OBJ_PROJ, OBJ_PANEL, OBJ_NOBJ };
/* flash: OBJ_PROJ 0xFC000, OBJ_PANEL 0xDC000 (2 sectors each, inside Felucca's data area) */

/* audio load in % of the render budget, and xruns since boot (device: from the ISR) */
uint32_t plat_cpu_pct(void);
uint32_t plat_xruns(void);

#if defined(BOARD_SMK37) && BOARD_SMK37
/* The SMK-37 build: the M-Vave SMK-37 Pro's extra controls (plat_fm1.c over hal/smk_input.h). plat_keys() above is
 * the FM-1's 27 note keys: keybed F3..G5 or a pad (the pads are the 16 white keys = the 16 strings). */
#define SMK_NKEYBED 37                 /* C3 .. C6 */
enum { SB_KNOB_BANK, SB_FADER_BANK, SB_LEFT, SB_RIGHT, NSB };   /* the four buttons the FM-1 has not */
uint32_t plat_smk_buttons(void);       /* held: bit SB_* */
void plat_smk_leds(uint32_t lit);      /* their LEDs, bit SB_* (before plat_leds) */
uint32_t plat_note_hits(void);         /* note-key presses since the last call, keybed or pad: bit k (0 = F3) */
uint32_t plat_pad_hits(void);          /* pad presses since the last call, as note-key bits */
uint32_t plat_pad_notes(void);         /* pads held, as note-key bits */
uint64_t plat_keybed(void);            /* keybed keys held: bit k = key k (0 = C3) */
uint64_t plat_keybed_hits(void);       /* keybed presses since the last call */
int plat_key_vel(int k);               /* velocity 1..127 of note key k's last press (keybed or pad) */
int plat_fader(int i);                 /* fader 0..3: 0..127, -1 while the readings settle */
int plat_wheels(int32_t *bend_q12, int32_t *modw);   /* 1 = new values: bend 1/4096 semitone, mod 0..127 */
void plat_pad_rgb(int string, uint8_t r, uint8_t g, uint8_t b);   /* the LED of string 0..15's pad */
#endif
