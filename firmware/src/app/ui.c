/* SPDX-License-Identifier: GPL-3.0-only */
/* OMNI UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too.
 *
 *   white keys     the strum plate: each one plucks a string of the chord
 *   black keys     the chord buttons; hold ENV for the minor, LFO for the 7th, both for the m7
 *   PLAY           the rhythm on / off (with SYNC START: it waits for the first chord)
 *   REC            SYNC START          ARP   CHORD HOLD          OCT- / OCT+   the plate's octave
 *   HOME SEQ FX SEL GLO   the pages: PLAY, RHYTHM, SOUND, CHORDS, SETUP (EDIT steps through them)
 *   SELECT tempo   ALGORITHM rhythm   PRESETS transpose   KNOB 1-4 the page's four values
 *   SAVE           saves (it also saves by itself, a few seconds after a change, when quiet)
 * The SMK-37 build (BOARD_SMK37, the M-Vave SMK-37 Pro): the extra controls, below ("SMK-37 Pro"). */

enum { V_PLAY, V_RHYTHM, V_SOUND, V_PADS, V_SETUP, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Play", "Rhythm", "Sound", "Chords", "Setup"};
/* the four knobs of each page: a parameter, or one of these */
enum { K_PADROOT = 100, K_PADTYPE, K_LEDS, K_NONE };
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {P_VOICE1, P_VOICE2, P_SUSTAIN, P_CHORD},
    {P_RHYTHM, P_TEMPO, P_RHYVOL, P_ABC},
    {P_REVERB, P_SPACE, P_WIDTH, P_CREV},
    {K_PADROOT, K_PADTYPE, P_TRANSPOSE, P_OCTAVE},
    {P_TUNE, P_MIDI, K_LEDS, K_NONE},
};

static const uint8_t WHITE_K[OM_NSTR] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t BLACK_K[NPADS] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};

/* the screen's colours: light and friendly. Cream paper, warm brown ink, one soft colour per section
 * (harp coral, chords teal, rhythm pink), each with a pale tint for the shapes at rest */
#define K_BG RGB(250, 244, 232)
#define K_PANEL RGB(236, 228, 212)
#define K_LINE RGB(216, 205, 186)
#define K_DIM RGB(150, 136, 118)
#define K_TEXT RGB(72, 58, 48)
#define K_WHITE RGB(255, 255, 255)
#define K_HARP RGB(242, 120, 72)
#define K_HARP_T RGB(248, 208, 184)
#define K_CHORD RGB(38, 166, 154)
#define K_CHORD_T RGB(196, 230, 222)
#define K_RHY RGB(232, 88, 128)
#define K_RHY_T RGB(246, 206, 218)

static struct {
    uint8_t view;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    int8_t pad;                        /* the chord button sounding (or held), -1 none, -2 a chord from MIDI */
    uint8_t pad_sel;                   /* the chord button the CHORDS page edits */
    uint8_t npads_down;
    uint8_t root, type;                /* the chord (before transpose) */
    uint8_t armed;                     /* SYNC START waiting for a chord */
    uint8_t dirty;                     /* the project changed since the last save */
    uint32_t act_t, saved_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;                    /* the knob turned last (0..3), shown bright for a moment */
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[3];                   /* what each band shows: redrawn when it changes */
} ui;

static void say(const char *a, const char *b)
{
    uint32_t i;
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
}
void ui_say(const char *a, const char *b) { say(a, b); }

static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static int str_len_(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

static void mark_dirty(void) { ui.dirty = 1; }

static uint32_t kb_held[4];                    /* MIDI in channel 2: the notes held (128) */
static int kb_any(void) { return (kb_held[0] | kb_held[1] | kb_held[2] | kb_held[3]) != 0; }
int ui_dirty(void) { return ui.dirty != 0; }

static void chord_name(int root, int type, char *b)
{
    const char *n = OM_NOTE_NAME[((root + proj.par[P_TRANSPOSE]) % 12 + 12) % 12], *t = OM_TYPE_NAME[type];
    while (*n)
        *b++ = *n++;
    while (*t)
        *b++ = *t++;
    *b = 0;
}

/* ------------------------------------------------------------ values --- */
static int knob_get(int k)
{
    if (k < P_NPARAMS)
        return proj.par[k];
    if (k == K_PADROOT)
        return proj.pad_root[ui.pad_sel];
    if (k == K_PADTYPE)
        return proj.pad_type[ui.pad_sel];
    if (k == K_LEDS)
        return proj.leds;
    return 0;
}
static void knob_range(int k, int *lo, int *hi)
{
    if (k < P_NPARAMS) {
        *lo = om_param_info(k)->lo;
        *hi = om_param_info(k)->hi;
    } else {
        *lo = 0;
        *hi = k == K_PADROOT ? 11 : k == K_PADTYPE ? CH_NTYPES - 1 : k == K_LEDS ? 2 : 0;
    }
}
static const char *knob_name(int k)
{
    if (k < P_NPARAMS)
        return om_param_info(k)->name;
    return k == K_PADROOT ? "Root" : k == K_PADTYPE ? "Type" : k == K_LEDS ? "Lights" : "";
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    if (k < P_NPARAMS) {
        om_param_text(k, v, b);
    } else if (k == K_PADROOT) {
        const char *n = OM_NOTE_NAME[v];
        while ((*b++ = *n++))
            ;
    } else if (k == K_PADTYPE) {
        const char *n = v ? OM_TYPE_NAME[v] : "maj";
        while ((*b++ = *n++))
            ;
    } else if (k == K_LEDS) {                    /* On: the keys, and the unlit buttons glow; Keys: no glow */
        const char *n = v == 2 ? "Keys" : v ? "On" : "Off";
        while ((*b++ = *n++))
            ;
    } else {
        b[0] = 0;
    }
}

static void sound_chord(void);
static void knob_set(int k, int v)
{
    int lo, hi;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == knob_get(k))
        return;
    if (k < P_NPARAMS) {
        proj.par[k] = (int16_t)v;
        om_set(k, v);
    } else if (k == K_PADROOT || k == K_PADTYPE) {
        if (k == K_PADROOT)
            proj.pad_root[ui.pad_sel] = (uint8_t)v;
        else
            proj.pad_type[ui.pad_sel] = (uint8_t)v;
        if (ui.pad == (int8_t)ui.pad_sel) {        /* the button being edited is the one sounding */
            ui.root = proj.pad_root[ui.pad_sel];
            ui.type = proj.pad_type[ui.pad_sel];
            om_chord(ui.root, ui.type);
        }
    } else if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
    }
    mark_dirty();
}

/* knob acceleration by speed: a slow turn is one step a detent, a quick one up to 6 */
static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (range <= 24 || !a || accel_off)
        return s;
    if (a > 1)
        dt /= a;
    m = dt < 15u ? 6u : dt < 30u ? 4u : dt < 60u ? 2u : 1u;
    return s * (int32_t)m;
}

static void turn(int role, int k, int32_t e)
{
    int lo, hi;
    if (!e || k == K_NONE)
        return;
    knob_range(k, &lo, &hi);
    knob_set(k, knob_get(k) + accel(role, e, hi - lo));
}

#if BOARD_SMK37
/* -------------------------------------------------------------- SMK-37 Pro --- */
/* The SMK-37 Pro's controls beyond the FM-1's (plat.h SMK section; the FM-1 ones keep their FoMni meaning):
 *   pads           the strum plate: the 16 strings with velocity (top row strings 1-8, bottom row 9-16)
 *   keybed HARP    (default) the middle 27 keys F3..G5 as on the FM-1: white keys pluck the strings, black keys
 *                  are the 11 chord buttons. An outer key held picks the chord type: C3 minor, D3 7th (both: m7,
 *                  as CHORD and NOTE REPEAT), E3 maj7, C#3 dim, D#3 aug, A5 sus4, B5 add9, C6 m7
 *   keybed CHORDS  (KNOB BANK) the Omnichord's chord buttons over the whole keybed: the key's note is the root,
 *                  C3..B3 major, C4..B4 minor, C5..B5 7th (CHORD / NOTE REPEAT add minor / 7th); C6 lets the
 *                  chord go. The strings are on the pads only
 *   FADER BANK     the four faders: A Voice 1, Voice 2, Chord, Drums; B Sustain, Reverb, Chord rev, Space
 *   < >            the previous / next page
 *   wheels         pitch bend +-2 semitones and the mod wheel's vibrato (strings, chord and bass) */
static struct {
    uint32_t sbtn;                     /* the four extra buttons held (SB_*) */
    uint64_t kb;                       /* keybed keys held */
    uint8_t fbank, kdown;              /* fader bank 0 A / 1 B; CHORDS keys down */
    uint8_t flast[4];                  /* each fader's last value, 0xFF: not read yet (takes over once it moves) */
} smk;
static const uint8_t SMK_FADER_PAR[2][4] = {{P_VOICE1, P_VOICE2, P_CHORD, P_RHYVOL},
                                            {P_SUSTAIN, P_REVERB, P_CREV, P_SPACE}};
static void smk_reset(void)
{
    memset(&smk, 0, sizeof smk);
    memset(smk.flast, 0xFF, sizeof smk.flast);
}
/* HARP: the chord type an outer key held picks, -1 none (keybed C3 = 0 .. C6 = 36) */
static int smk_type(void)
{
    uint64_t k = smk.kb;
    if (proj.keybed & 1u)
        return -1;
    if ((k & 5u) == 5u)
        return CH_M7;                              /* C3 + D3 */
    if (k & 1u)
        return CH_MIN;                             /* C3 */
    if (k >> 2 & 1u)
        return CH_7;                               /* D3 */
    if (k >> 4 & 1u)
        return CH_MAJ7;                            /* E3 */
    if (k >> 1 & 1u)
        return CH_DIM;                             /* C#3 */
    if (k >> 3 & 1u)
        return CH_AUG;                             /* D#3 */
    if (k >> 33 & 1u)
        return CH_SUS4;                            /* A5 */
    if (k >> 35 & 1u)
        return CH_ADD9;                            /* B5 */
    if (k >> 36 & 1u)
        return CH_M7;                              /* C6 */
    return -1;
}
#endif

/* ------------------------------------------------------------- chords --- */
static void sound_chord(void) { om_chord(ui.root, ui.type); }

static void pad_down(int p)
{
    uint32_t b = ui.btn;
    int type = proj.pad_type[p];
    int mods = (b >> B_ENV & 1u) | (b >> B_LFO & 1u) << 1;
    if (mods)                                    /* the OM's rows: minor, 7th, both: m7 */
        type = mods == 1 ? CH_MIN : mods == 2 ? CH_7 : CH_M7;
#if BOARD_SMK37
    {
        int t = smk_type();                      /* an outer key held (HARP) */
        if (t >= 0)
            type = t;
    }
#endif
    if (mods & 1)
        ui.btn_used |= 1u << B_ENV;
    if (mods & 2)
        ui.btn_used |= 1u << B_LFO;
    if (ui.view == V_PADS)
        ui.pad_sel = (uint8_t)p;
    if (proj.hold && ui.pad == p && !ui.npads_down && ui.root == proj.pad_root[p] && ui.type == type) {
        ui.pad = -1;                             /* HOLD: the sounding button again lets it go */
        om_gate(0);
        ui.npads_down++;
        return;
    }
    ui.npads_down++;
    ui.pad = (int8_t)p;
    ui.root = proj.pad_root[p];
    ui.type = (uint8_t)type;
    sound_chord();
    om_gate(1);
    if (ui.armed) {                              /* SYNC START: the rhythm starts with the chord */
        ui.armed = 0;
        om_play(1);
    }
}

static void pad_up(int p)
{
    if (ui.npads_down)
        ui.npads_down--;
    if (ui.npads_down || proj.hold)
        return;
    if (ui.pad == p || ui.pad >= 0) {
        om_gate(0);
        ui.pad = -1;
    }
}

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    ui.view = (uint8_t)v;
    ui.touched = -1;
}

static void button(int b)
{
    switch (b) {
    case B_HOME: set_view(V_PLAY); break;
    case B_SEQ: set_view(V_RHYTHM); break;
    case B_FX: set_view(V_SOUND); break;
    case B_SEL: set_view(V_PADS); break;
    case B_GLO: set_view(V_SETUP); break;
    case B_EDIT: set_view((ui.view + 1) % NVIEWS); break;
    case B_PLAY:
        if (om_playing) {
            om_play(0);
            ui.armed = 0;
        } else if (ui.armed) {
            ui.armed = 0;
            say("SYNC START", "CANCELLED");
        } else if (proj.sync && ui.pad == -1) {
            ui.armed = 1;
            say("WAITING FOR", "A CHORD");
        } else {
            om_play(1);
        }
        break;
    case B_REC:
        proj.sync = (uint8_t)!proj.sync;
        if (!proj.sync)
            ui.armed = 0;
        say("SYNC START", proj.sync ? "ON" : "OFF");
        mark_dirty();
        break;
    case B_ARP:
        proj.hold = (uint8_t)!proj.hold;
        if (!proj.hold && !ui.npads_down && ui.pad != -1 && !(ui.pad == -2 && kb_any())) {
            om_gate(0);
            ui.pad = -1;
        }
        say("CHORD HOLD", proj.hold ? "ON" : "OFF");
        mark_dirty();
        break;
    case B_OCTDN: case B_OCTUP:
        knob_set(P_OCTAVE, proj.par[P_OCTAVE] + (b == B_OCTUP ? 1 : -1));
        {
            char t[8];
            om_param_text(P_OCTAVE, proj.par[P_OCTAVE], t);
            say("OCTAVE", t);
        }
        break;
    case B_SAVE:
        if (project_save() == 0) {
            ui.dirty = 0;
            say("SAVED", 0);
        } else {
            say("NOT SAVED", "FLASH ERROR");
        }
        break;
    default: break;
    }
}

/* MIDI in, channel 2: the chord held on a keyboard. The best fit of the nine types: a root among the
 * notes (the lowest preferred), as many of its tones held as possible and nothing left over but its
 * fifth (a held C E G Bb is C7, which the OM plays without the G). Under two notes: no chord. */
static int kb_chord(int *root, int *type)
{
    uint32_t pcs = 0, n;
    int low = -1, r, t, best = -1;
    for (n = 0; n < 128u; n++)
        if (kb_held[n >> 5] >> (n & 31u) & 1u) {
            pcs |= 1u << (n % 12u);
            if (low < 0)
                low = (int)(n % 12u);
        }
    if (low < 0 || !(pcs & (pcs - 1u)))
        return 0;
    for (r = 0; r < 12; r++) {
        if (!(pcs >> r & 1u))
            continue;
        for (t = 0; t < CH_NTYPES; t++) {
            uint32_t tones = 0, k;
            int score, hit = 0, extra = 0;
            for (k = 0; k < 3u; k++)
                tones |= 1u << ((r + OM_TYPE_TONES[t][k]) % 12);
            for (k = 0; k < 12u; k++) {
                hit += (pcs & tones) >> k & 1u;
                extra += (pcs & ~tones & ~(1u << ((r + 7) % 12))) >> k & 1u;
            }
            score = hit * 10 - extra * 3 + (r == low) * 2 - t / 4;   /* the common types first, on a tie */
            if (score > best) {
                best = score;
                *root = r;
                *type = t;
            }
        }
    }
    return best >= 20;
}

static void kb_update(void)
{
    int root, type;
    if (kb_chord(&root, &type)) {
        if (ui.pad == -2 && ui.root == root && ui.type == type)
            return;
        ui.root = (uint8_t)root;
        ui.type = (uint8_t)type;
        ui.pad = -2;                             /* a chord from MIDI: no button lit */
        sound_chord();
        om_gate(1);
        if (ui.armed) {
            ui.armed = 0;
            om_play(1);
        }
    } else if (ui.pad == -2 && !kb_any() && !proj.hold) {
        ui.pad = -1;                             /* all let go */
        om_gate(0);
    }
}

/* MIDI in (USB and the TRS jack): channel 1 notes pluck the nearest string, channel 2 notes pick the
 * chord, clock and start / stop drive the rhythm */
static void midi_in(void)
{
    uint32_t pkt;
    int chords = 0;
    while (plat_midi_in(&pkt)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu) {
            om_clock(st == 0xF8u ? OM_CLK_TICK : st == 0xFAu ? OM_CLK_START : st == 0xFBu ? OM_CLK_CONTINUE : OM_CLK_STOP);
            if (st == 0xFAu)
                ui.armed = 0;
        } else if (st == 0x90u && v) {
            int s, best = 0, bd = 999;
            for (s = 0; s < OM_NSTR; s++) {
                int d = om_str_note[s] - (int)n;
                d = d < 0 ? -d : d;
                if (d < bd) {
                    bd = d;
                    best = s;
                }
            }
            om_strum(best);
        } else if (st == 0x91u || st == 0x81u) {
            if (st == 0x91u && v)
                kb_held[n >> 5] |= 1u << (n & 31u);
            else
                kb_held[n >> 5] &= ~(1u << (n & 31u));
            chords = 1;
        }
    }
    if (chords)
        kb_update();
}

#if BOARD_SMK37
/* CHORDS keybed: key k (0 = C3) pressed / let go */
static void smk_chord_down(int k)
{
    uint32_t b = ui.btn;
    int root = k % 12, row = k / 12, minor, sev, type;
    if (row > 2) {                               /* C6: let the chord go (also a held one) */
        if (ui.pad != -1) {
            om_gate(0);
            ui.pad = -1;
        }
        return;
    }
    minor = row == 1 || (b >> B_ENV & 1u);
    sev = row == 2 || (b >> B_LFO & 1u);
    type = minor && sev ? CH_M7 : minor ? CH_MIN : sev ? CH_7 : CH_MAJ;
    if (b >> B_ENV & 1u)
        ui.btn_used |= 1u << B_ENV;
    if (b >> B_LFO & 1u)
        ui.btn_used |= 1u << B_LFO;
    if (proj.hold && ui.pad == -3 && !smk.kdown && ui.root == root && ui.type == type) {
        ui.pad = -1;                             /* HOLD: the sounding chord again lets it go */
        om_gate(0);
        smk.kdown++;
        return;
    }
    smk.kdown++;
    ui.pad = -3;                                 /* a chord from the keybed: no chord button lit */
    ui.root = (uint8_t)root;
    ui.type = (uint8_t)type;
    sound_chord();
    om_gate(1);
    if (ui.armed) {                              /* SYNC START */
        ui.armed = 0;
        om_play(1);
    }
}
static void smk_chord_up(int k)
{
    if (k / 12 > 2)
        return;
    if (smk.kdown)
        smk.kdown--;
    if (smk.kdown || proj.hold)
        return;
    if (ui.pad == -3) {
        om_gate(0);
        ui.pad = -1;
    }
}

/* the SMK's controls, each input pass: *keys gets the note keys the FM-1 code sees (CHORDS: the pads only),
 * *hits the strings plucked since the last pass (latched edges: a quick strum is never lost) */
static void smk_input(uint32_t *keys, uint32_t *hits)
{
    uint32_t sb = plat_smk_buttons(), sp = sb & ~smk.sbtn, nh = plat_note_hits(), ph = plat_pad_hits(), i;
    uint64_t kb = plat_keybed(), kh = plat_keybed_hits();
    int32_t bend, modw;
    smk.sbtn = sb;
    if (sp >> SB_KNOB_BANK & 1u) {               /* KNOB BANK: the keybed HARP / CHORDS */
        proj.keybed = (uint8_t)!(proj.keybed & 1u);
        smk.kdown = 0;
        if (ui.pad < -2 && !proj.hold) {         /* a keybed chord sounding: let it go */
            om_gate(0);
            ui.pad = -1;
        }
        say("KEYBED", proj.keybed ? "CHORDS" : "HARP");
        mark_dirty();
    }
    if (sp >> SB_FADER_BANK & 1u) {              /* FADER BANK: A levels / B sound */
        smk.fbank ^= 1u;
        memset(smk.flast, 0xFF, sizeof smk.flast);
        say("FADER BANK", smk.fbank ? "B SOUND" : "A LEVELS");
    }
    if (sp >> SB_LEFT & 1u)
        set_view((ui.view + NVIEWS - 1) % NVIEWS);
    if (sp >> SB_RIGHT & 1u)
        set_view((ui.view + 1) % NVIEWS);
    if (proj.keybed & 1u) {                      /* CHORDS: every key a chord button */
        int k;
        for (k = 0; k < SMK_NKEYBED; k++) {
            int was = (int)(smk.kb >> k & 1u), now = (int)(kb >> k & 1u), hit = (int)(kh >> k & 1u);
            if (was && (hit || !now))
                smk_chord_up(k);
            if (hit) {
                smk_chord_down(k);
                if (!now)                         /* pressed and let go between two passes */
                    smk_chord_up(k);
            }
        }
        *keys = plat_pad_notes();
        *hits = ph;
    } else {
        *hits = nh;                               /* keybed white keys and pads */
    }
    smk.kb = kb;
    if (sb || kb || ph || nh)
        ui.act_t = plat_ms();
    for (i = 0; i < 4u; i++) {                   /* the faders: a parameter each, once a fader moves */
        int v = plat_fader((int)i), k = SMK_FADER_PAR[smk.fbank][i], lo, hi;
        char t[16];
        if (v < 0)
            continue;
        if (smk.flast[i] == 0xFFu) {
            smk.flast[i] = (uint8_t)v;
            continue;
        }
        if (v == smk.flast[i])
            continue;
        smk.flast[i] = (uint8_t)v;
        knob_range(k, &lo, &hi);
        knob_set(k, lo + (v * (hi - lo) + 63) / 127);
        knob_text(k, t);
        say(knob_name(k), t);
        ui.act_t = plat_ms();
    }
    if (plat_wheels(&bend, &modw))
        om_perf(bend, modw);
}

/* the pads' LEDs: each string lights while it rings (the harp colour); at rest the roots glow, as the plate's
 * dots on the screen. Lights Off: dark. */
static void smk_pads(void)
{
    int s, root = ((ui.root + proj.par[P_TRANSPOSE]) % 12 + 12) % 12;
    for (s = 0; s < OM_NSTR; s++) {
        uint32_t r = 0, g = 0, b = 0, q;
        if (proj.leds) {
            if (om_str_note[s] % 12 == root) {
                r = 28;
                g = 10;
                b = 4;
            }
            q = (uint32_t)(om_str_level[s] * 160.0f);
            if (q > 160u)
                q = 160u;
            if (q > 4u && (242u * q >> 8) > r) {
                r = 242u * q >> 8;
                g = 120u * q >> 8;
                b = 72u * q >> 8;
            }
        }
        plat_pad_rgb(s, (uint8_t)r, (uint8_t)g, (uint8_t)b);
    }
}
#endif

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
#if BOARD_SMK37
    uint32_t hits;
    smk_input(&keys, &hits);
#endif
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            if (i == B_PLAY)                   /* transport on the press */
                button((int)i);
        } else if (!(ui.btn_used & m) && i != B_PLAY && i != B_ENV && i != B_LFO) {
            button((int)i);
        }
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
#if BOARD_SMK37
    for (i = 0; i < OM_NSTR; i++)              /* strings first, with the key's or the pad's velocity */
        if (hits >> WHITE_K[i] & 1u)
            om_strum_vel((int)i, plat_key_vel(WHITE_K[i]));
#else
    for (i = 0; i < OM_NSTR; i++)              /* strings first: a strum is the fastest thing here */
        if ((ch & keys) >> WHITE_K[i] & 1u)
            om_strum((int)i);
#endif
    for (i = 0; i < NPADS; i++)
        if (ch >> BLACK_K[i] & 1u) {
            if (keys >> BLACK_K[i] & 1u)
                pad_down((int)i);
            else
                pad_up((int)i);
        }
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = plat_enc(EN_SELECT)) != 0)
        turn(EN_SELECT, P_TEMPO, e);
    if ((e = plat_enc(EN_ALGO)) != 0)
        knob_set(P_RHYTHM, proj.par[P_RHYTHM] + (e > 0 ? 1 : -1));
    if ((e = plat_enc(EN_PRESET)) != 0)
        knob_set(P_TRANSPOSE, proj.par[P_TRANSPOSE] + (e > 0 ? 1 : -1));
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            turn(EN_K1 + (int)i, VIEW_KNOB[ui.view][i], e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = plat_ms();
        }
}

/* ------------------------------------------------------------- screen --- */
/* three bands: the header (28 px), the instrument (144), the knobs (68) */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 144
#define KNB_Y 172
#define KNB_H 68

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/* a rectangle with rounded corners (radius r, up to 6) */
static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

static void draw_header(void)
{
    char t[24];
    int msg = plat_ms() < ui.msg_until;
    uint32_t h = hash(hash(hash(2166136261u, ui.view), om_playing | ui.armed << 1 | proj.hold << 2 | proj.sync << 3 |
                                                       ui.dirty << 4 | (uint32_t)msg << 5),
                      (uint32_t)proj.par[P_RHYTHM] << 8 | (uint32_t)proj.par[P_TEMPO] | (uint32_t)om_ext << 20);
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    if (om_playing)
        h = hash(h, om_step / 4u);
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_HARP);
    } else {
        int32_t x = 10;
        cv_text(x, 6, &FONT_B, VIEW_NAME[ui.view], K_TEXT);
        x = 232;
        {   /* right: tempo, the rhythm, its state */
            if (om_ext) {                         /* following MIDI clock */
                t[0] = 'E';
                t[1] = 'X';
                t[2] = 'T';
                t[3] = 0;
            } else {
                itoa_u((uint32_t)proj.par[P_TEMPO], t);
            }
            x -= text_w(&FONT_S, t);
            cv_text(x, 6, &FONT_S, t, om_playing ? K_TEXT : K_DIM);
            x -= 8 + text_w(&FONT_S, om_rhythm(proj.par[P_RHYTHM])->name);
            cv_text(x, 6, &FONT_S, om_rhythm(proj.par[P_RHYTHM])->name, om_playing ? K_RHY : K_DIM);
            x -= 16;
            if (om_playing) {                     /* a beat light */
                int on = (om_step & 3u) < 2u;
                cv_round(x, 8, 10, 10, 5, on ? K_RHY : K_RHY_T);
            } else if (ui.armed) {
                cv_round(x, 8, 10, 10, 5, (ui.frame / 8u) & 1u ? K_RHY : K_RHY_T);
            } else {
                int32_t j;                        /* stopped: a play triangle */
                for (j = 0; j < 5; j++)
                    cv_rect(x + j, 9 + j, 1, 11 - 2 * j, K_DIM);
            }
        }
        x = 18 + text_w(&FONT_B, VIEW_NAME[ui.view]);
        if (proj.hold) {                          /* little tags */
            int32_t w = text_w(&FONT_XS, "HOLD") + 10;
            cv_round(x, 6, w, 16, 6, K_CHORD_T);
            cv_text(x + 5, 7, &FONT_XS, "HOLD", K_CHORD);
            x += w + 4;
        }
        if (proj.sync) {
            int32_t w = text_w(&FONT_XS, "SYNC") + 10;
            cv_round(x, 6, w, 16, 6, K_RHY_T);
            cv_text(x + 5, 7, &FONT_XS, "SYNC", K_RHY);
            x += w + 4;
        }
        if (ui.dirty)
            cv_round(x, 12, 5, 5, 2, K_DIM);
    }
    cv_blit(0, 0);
}

static uint16_t mix(uint16_t a, uint16_t b, int t)   /* t 0..16: a -> b */
{
    int r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16, g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16,
        bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}

static void draw_strings(int32_t y0, int32_t h)
{
    int s;
    for (s = 0; s < OM_NSTR; s++) {
        float lv = om_str_level[s];
        int t = (int)(lv * 16.0f + 0.5f), x = 9 + s * 14, w = 1 + (int)(lv * 4.0f + 0.3f);
        uint16_t c = mix(K_HARP_T, K_HARP, t > 16 ? 16 : t);
        int amp = (int)(lv * 3.0f), y;
        if (amp && (ui.frame & 1u))
            amp = -amp;
        for (y = 0; y < h; y++) {                  /* a string: bowed out in the middle while it rings */
            int d = (y < h / 2 ? y : h - 1 - y) * amp * 2 / h;
            cv_rect(x + 6 - w / 2 + d, y0 + y, w, 1, c);
        }
        if ((om_str_note[s] % 12) == (uint8_t)(((ui.root + proj.par[P_TRANSPOSE]) % 12 + 12) % 12))
            cv_round(x + 4, y0 + h + 3, 5, 5, 2, t > 2 ? K_HARP : K_LINE);   /* the roots: the plate's dots */
    }
}

static void draw_rhythm(int32_t y0, int32_t h)
{
    const om_rhythm_t *r = om_rhythm(proj.par[P_RHYTHM]);
    int len = r->len, s, d, cw = 224 / len;
    int32_t x0 = 120 - cw * len / 2, rowh = h / 6;
    static const char *const DN[5] = {"BD", "CL", "HH", "CY", "SD"};
    for (d = 0; d < 6; d++) {
        for (s = 0; s < len; s++) {
            int on, cur = om_playing && om_step == s;
            uint16_t c;
            if (d < 5) {
                on = r->drum[s] >> d & 1u;
                c = on ? (cur ? K_TEXT : K_RHY) : (s % 4 ? K_PANEL : K_RHY_T);
            } else {                               /* the bass line */
                on = r->bass[s] == 'T' || r->bass[s] == 'O';
                c = on ? (cur ? K_TEXT : K_CHORD) : (s % 4 ? K_PANEL : K_CHORD_T);
            }
            cv_round(x0 + s * cw, y0 + d * rowh, cw - 1, rowh - 2, 2, c);
        }
        (void)DN;
    }
}

static void draw_pads(int32_t y0)
{
    int p;
    for (p = 0; p < NPADS; p++) {
        char t[12];
        int32_t x = 4 + p * 20 + ((p >= 3) + (p >= 5) + (p >= 8) + (p >= 10)) * 3;   /* grouped as the keys are */
        int on = ui.pad == p, sel = ui.view == V_PADS && ui.pad_sel == p;
        if (sel)
            cv_round(x - 1, y0 - 1, 20, 30, 6, K_TEXT);
        cv_round(x, y0, 18, 28, 5, on ? K_CHORD : K_CHORD_T);
        chord_name(proj.pad_root[p], 0, t);
        text_c(x + 9, y0 + 1, &FONT_XS, t, on ? K_WHITE : K_TEXT);
        text_c(x + 9, y0 + 13, &FONT_XS, proj.pad_type[p] ? OM_TYPE_NAME[proj.pad_type[p]] : "", on ? K_WHITE : K_CHORD);
    }
}

static void draw_main(void)
{
    char name[16], notes[24];
    uint32_t h = hash(2166136261u, ui.view), s;
    for (s = 0; s < OM_NSTR; s++)
        h = hash(h, (uint32_t)(om_str_level[s] * 24.0f) | (uint32_t)om_str_note[s] << 8);
    h = hash(hash(h, (uint32_t)ui.pad | ui.pad_sel << 8 | ui.root << 16 | ui.type << 24),
             (uint32_t)proj.par[P_TRANSPOSE] << 8 | (uint32_t)(om_chord_level * 8.0f));
    for (s = 0; s < NPADS; s++)
        h = hash(h, proj.pad_root[s] | proj.pad_type[s] << 4);
    if (ui.view == V_RHYTHM)
        h = hash(hash(h, om_playing ? om_step : 255u), (uint32_t)proj.par[P_RHYTHM]);
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    chord_name(ui.root, ui.type, name);
    {   /* the chord: its name, big, and its notes */
        uint8_t hp[OM_NSTR], cn[OM_NCHORD], bn;
        int i, k = 0;
        uint16_t c = mix(K_TEXT, K_CHORD, (int)(om_chord_level * 16.0f));
        int32_t x = 12;
        const char *tn = OM_TYPE_NAME[ui.type];
        name[str_len_(name) - str_len_(tn)] = 0;        /* the root, big; the type beside it */
        x = cv_text(x, 4, &FONT_L, name, c);
        cv_text(x + 2, 16, &FONT_B, tn, c);
        om_voicing(ui.root, ui.type, proj.par[P_TRANSPOSE], proj.par[P_OCTAVE], hp, cn, &bn);
        for (i = 0; i < 3; i++) {
            const char *n = OM_NOTE_NAME[cn[i] % 12];
            if (i)
                notes[k++] = ' ';
            while (*n)
                notes[k++] = *n++;
        }
        notes[k] = 0;
        cv_text(240 - 12 - text_w(&FONT_S, notes), 14, &FONT_S, notes, K_DIM);
    }
    if (ui.view == V_RHYTHM)
        draw_rhythm(44, 62);
    else
        draw_strings(44, 54);
    draw_pads(112);
    cv_blit(0, MAIN_Y);
}

static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8 | ui.pad_sel << 16), i;
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(VIEW_KNOB[ui.view][i]));
    h = hash(h, (uint32_t)now_touch);
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    cv_round(4, 2, 232, KNB_H - 6, 6, K_PANEL);
    for (i = 0; i < 4; i++) {
        int k = VIEW_KNOB[ui.view][i], lo, hi, v;
        int32_t x = (int32_t)i * 60, cx = x + 30, bw;
        int hot = now_touch && ui.touched == (int)i;
        char t[16];
        if (k == K_NONE)
            continue;
        if (i)
            cv_rect(x, 14, 1, KNB_H - 30, K_LINE);
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        text_c(cx, 6, &FONT_XS, knob_name(k), hot ? K_TEXT : K_DIM);
        knob_text(k, t);
        {   /* the value: big, unless it has lower case (the big face has none) or is long */
            const char *q = t;
            int lower = 0;
            for (; *q; q++)
                lower |= *q >= 'a' && *q <= 'z';
            if (lower || text_w(&FONT_M, t) > 56)
                text_c(cx, 26, &FONT_B, t, hot ? K_HARP : K_TEXT);
            else
                text_c(cx, 22, &FONT_M, t, hot ? K_HARP : K_TEXT);
        }
        bw = hi > lo ? (int32_t)(44 * (v - lo) / (hi - lo)) : 0;
        cv_round(cx - 22, 50, 44, 6, 3, K_LINE);
        if (lo < 0) {                                /* centred values: a bar from the middle */
            int32_t m = cx - 22 + 44 * (0 - lo) / (hi - lo), e = cx - 22 + bw;
            cv_round(e < m ? e : m, 50, (e < m ? m - e : e - m) + 6, 6, 3, hot ? K_HARP : K_CHORD);
        } else if (bw) {
            cv_round(cx - 22, 50, bw < 6 ? 6 : bw, 6, 3, hot ? K_HARP : K_CHORD);
        }
    }
    cv_blit(0, KNB_Y);
}

static void leds(void)
{
    uint32_t b = 0, k = 0, i;
    static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_SEQ, B_FX, B_SEL, B_GLO};
    b |= 1u << VIEW_BTN[ui.view];
    if (om_playing)
        b |= (om_step & 3u) < 2u ? 1u << B_PLAY : 0u;
    else if (ui.armed && (ui.frame / 8u) & 1u)
        b |= 1u << B_PLAY;
    if (proj.sync)
        b |= 1u << B_REC;
    if (proj.hold)
        b |= 1u << B_ARP;
    if (proj.leds) {
        if (ui.pad >= 0)
            k |= 1u << BLACK_K[ui.pad];
        for (i = 0; i < OM_NSTR; i++)
            if (om_str_level[i] > 0.2f)
                k |= 1u << WHITE_K[i];
    }
    plat_glow(proj.leds == 1);
#if BOARD_SMK37
    plat_smk_leds((proj.keybed & 1u) << SB_KNOB_BANK | (uint32_t)smk.fbank << SB_FADER_BANK);
    smk_pads();
#endif
    plat_leds(b, k);
}

/* AUTOSAVE: a few seconds after a change, when nothing sounds and nothing is touched (a flash erase
 * silences the audio for a moment) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms(), i;
    if (!ui.dirty || om_playing || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || om_chord_level > 0.0f ||
        om_bass_level > 0.0f)
        return;
    for (i = 0; i < OM_NSTR; i++)
        if (om_str_level[i] > 0.0f)
            return;
    if (project_save() == 0) {
        ui.dirty = 0;
        ui.saved_t = now;
    }
}

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
#if BOARD_SMK37
    smk_reset();
#endif
    ui.pad = -1;
    ui.touched = -1;
    ui.root = proj.pad_root[1];                  /* C, the second button */
    ui.type = proj.pad_type[1];
    sound_chord();
    lcd_fill(0, 0, 240, 240, K_BG);
}

void ui_frame(void)
{
    input();
    autosave();
    draw_header();
    draw_main();
    draw_knobs();
    leds();
    ui.frame++;
}

void ui_input_only(void) { input(); }
