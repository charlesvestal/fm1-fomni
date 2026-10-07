/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument (omni.h). One compilation unit, -O2. Waves, envelope shapes, voicing and rhythms
 * after Chordian (Jan125/pb.chordian, CC0 1.0); its rates were written for 96 kHz and are converted
 * here where they are per sample (the filters), not where they are per second (the envelopes). */
#include "omni.h"
#include "fastmath.h"
#include "om_data.h"
#include "om_drums.h"

#ifdef OM_HOST
#define OM_POOL
#else
#define OM_POOL __attribute__((section(".pool")))
#endif
#define BARRIER() __asm__ volatile("" ::: "memory")

const char *const OM_TYPE_NAME[CH_NTYPES] = {"", "m", "7", "M7", "m7", "dim", "aug", "sus4", "add9"};
const char *const OM_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
/* the three tones (semitones over the root); the third tone of a 7th is the 7th, as on the OM */
const int8_t OM_TYPE_TONES[CH_NTYPES][3] = {
    {0, 4, 7}, {0, 3, 7}, {0, 4, 10}, {0, 4, 11}, {0, 3, 10}, {0, 3, 6}, {0, 4, 8}, {0, 5, 7}, {0, 4, 2}};

static const om_param_t PARAMS[P_NPARAMS] = {
    {"Voice 1", 0, 100, 80}, {"Voice 2", 0, 100, 45}, {"Sustain", 0, 100, 50}, {"Chord", 0, 100, 60},
    {"Rhythm", 0, OM_NRHYTHM - 1, 0}, {"Tempo", 60, 240, 116}, {"Drums", 0, 100, 70}, {"Auto bass", 0, 1, 1},
    {"Reverb", 0, 100, 30}, {"Space", 0, 100, 55}, {"Tune", -50, 50, 0}, {"Chord rev", 0, 100, 25},
    {"Transpose", -6, 6, 0}, {"Octave", -1, 1, 0}, {"Width", 0, 100, 60}, {"MIDI out", 0, 1, 1},
};

const om_rhythm_t *om_rhythm(int i) { return &OM_RHYTHM[(unsigned)i < OM_NRHYTHM ? i : 0]; }

const om_param_t *om_param_info(int i) { return (i >= 0 && i < P_NPARAMS) ? &PARAMS[i] : &PARAMS[0]; }

static void itoa_s(int v, char *b)
{
    char t[8];
    int n = 0, neg = v < 0;
    if (neg)
        v = -v;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 6);
    if (neg)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

void om_param_text(int i, int v, char *b)
{
    static const char *const ONOFF[2] = {"Off", "On"};
    const char *s = 0;
    if (i == P_RHYTHM)
        s = OM_RHYTHM[v < 0 ? 0 : v % OM_NRHYTHM].name;
    else if (i == P_ABC || i == P_MIDI)
        s = ONOFF[v ? 1 : 0];
    if (s) {
        int k = 0;
        while (s[k] && k < 9) {
            b[k] = s[k];
            k++;
        }
        b[k] = 0;
        return;
    }
    if ((i == P_TRANSPOSE || i == P_OCTAVE || i == P_TUNE) && v > 0) {
        *b++ = '+';
    }
    itoa_s(v, b);
}

/* ------------------------------------------------------------- voicing --- */
static int fold(int pc, int lo) { return lo + (((pc - lo) % 12) + 12) % 12; }

void om_voicing(int root, int type, int transpose, int octave, uint8_t harp[OM_NSTR], uint8_t chord[OM_NCHORD],
                uint8_t *bass)
{
    const int8_t *t = OM_TYPE_TONES[(unsigned)type < CH_NTYPES ? type : 0];
    int r = root + transpose, k, i;
    for (k = 0; k < 5; k++)                       /* windows F#2..F3, F#3..F4, ...: root, 2nd, 3rd tone */
        for (i = 0; i < 3; i++)
            harp[3 * k + i] = (uint8_t)fold(r + t[i], 42 + 12 * k + 12 * octave);
    harp[15] = (uint8_t)fold(r, 42 + 60 + 12 * octave);
    for (i = 0; i < 3; i++)
        chord[i] = (uint8_t)fold(r + t[i], 54);  /* F#3..F4, the OM's chord register */
    *bass = (uint8_t)fold(r, 36);                 /* C2..B2 */
}

/* ------------------------------------------------------------ commands --- */
enum { C_SET, C_CHORD, C_GATE, C_STRUM, C_PLAY, C_PANIC, C_CLOCK };
#define QN 64u
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
static void post(uint32_t c, int a, int b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN)
        return;                                   /* full: dropped (never, at the UI's rate) */
    q[w % QN] = c << 24 | ((uint32_t)a & 0xFFu) << 16 | ((uint32_t)b & 0xFFFFu);
    BARRIER();
    q_w = w + 1;
}
void om_set(int p, int v) { post(C_SET, p, v); }
void om_chord(int root, int type) { post(C_CHORD, root, type); }
void om_gate(int on) { post(C_GATE, on, 0); }
void om_strum(int s) { post(C_STRUM, s, 0); }
void om_play(int on) { post(C_PLAY, on, 0); }
void om_panic(void) { post(C_PANIC, 0, 0); }
void om_clock(int msg) { post(C_CLOCK, msg, 0); }

/* -------------------------------------------------------------- state --- */
volatile float om_str_level[OM_NSTR];
volatile uint8_t om_str_note[OM_NSTR];        /* the note a pluck plays now */
volatile float om_chord_level, om_bass_level;
volatile uint8_t om_playing, om_step, om_steps = 32;
volatile uint8_t om_drum_hit;
volatile uint8_t om_ext;

static int16_t par[P_NPARAMS];
static float tunefac = 1.0f, v1, v2, cvol, rvol, rsend, csend, sus_rate;
static float pan_l[OM_NSTR], pan_r[OM_NSTR];
static int root, type;
static uint8_t harp_n[OM_NSTR], chord_n[OM_NCHORD], bass_n;
static int gate;

static float sin3[360];                       /* Chordian's shimmer shape: |sin|^0.6 */
static float shim_ph, shim_rate;
/* the shimmer's rate follows the chord's root (Chordian, Hz, C..B) */
static const float SHIM_HZ[12] = {3.761f, 2.910f, 4.275f, 4.565f, 5.050f, 6.333f, 2.843f, 3.125f, 3.162f, 3.419f,
                                  3.667f, 3.953f};

static float note_inc(int n)                   /* wave positions (100 per cycle) per sample */
{
    return 440.0f * tunefac * fm_exp2f((float)(n - 69) * (1.0f / 12.0f)) * (100.0f / OM_SR);
}

static inline float wave(const float *w, float pos)
{
    int i = (int)pos;
    float f = pos - (float)i, a = w[i], b = w[i + 1 < 100 ? i + 1 : 0];
    return a + (b - a) * f;
}
static inline float adv(float pos, float inc)
{
    pos += inc;
    while (pos >= 100.0f)
        pos -= 100.0f;
    return pos;
}

/* ---- harp strings */
typedef struct {
    float pos, inc, v, g, y1, y2, k, c, m, modamt;
    uint16_t shim_off;
    uint8_t on, note;
} str_t;
static str_t str[OM_NSTR];

static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (par[P_MIDI])
        om_midi_out(st, d1, d2);
}

static void strum(int s)
{
    str_t *v = &str[s];
    int n = harp_n[s];
    float u = fm_exp2f((float)(n - 69) * (1.0f / 12.0f)) * tunefac, base;
    if (v->on && v->note != n)
        midi(0x80, v->note, 0);
    if (v->on)
        midi(0x80, n, 0);
    midi(0x90, (uint32_t)n, 100);
    if (!v->on) {
        v->y1 = v->y2 = 0.0f;
        v->g = 0.0f;
    }
    v->on = 1;
    v->note = (uint8_t)n;
    v->inc = note_inc(n);
    v->v = 1.0f;
    /* the voice filter: Chordian's per-note brightness, darker as the note decays */
    base = fm_maxf(((18.0f - u) / 18.0f) * 0.31f + 0.34f, 0.05f);
    v->k = fm_powf(base, 2.85f);
    {   /* the shimmer: less on the higher strings; each string its own phase (Chordian's offsets) */
        int i = s * 12 / 15;
        v->modamt = 1.0f - (float)i / 17.0f;
        v->shim_off = (uint16_t)(((i & 1) ? 27 * i : 14 * i) % 360);
    }
}

/* ---- chord and bass */
enum { E_OFF, E_DEC, E_SUS, E_REL, E_BDEC, E_CUT };
typedef struct {
    float pos, inc, v, g;
    uint8_t st, note;
} cv_t;
static cv_t chv[OM_NCHORD], bsv;
static uint8_t bass_kind;                      /* 0 root, 1 fifth, 2 third */

static int bass_note(void)
{
    const int8_t *t = OM_TYPE_TONES[type];
    return bass_n + (bass_kind == 1 ? 7 : bass_kind == 2 ? t[1] : 0);
}

static void chord_cmd(char c)
{
    int i;
    if (c == 'T' || c == 'O') {
        for (i = 0; i < OM_NCHORD; i++) {
            if (chv[i].st != E_OFF)
                midi(0x81, chv[i].note, 0);
            chv[i].note = chord_n[i];
            chv[i].inc = note_inc(chord_n[i]);
            chv[i].v = 1.0f;
            chv[i].st = c == 'T' ? E_DEC : E_REL;
            midi(0x91, chord_n[i], 90);
        }
    } else if (c == 'R' || c == 'N') {
        for (i = 0; i < OM_NCHORD; i++)
            if (chv[i].st != E_OFF)
                chv[i].st = c == 'R' ? E_REL : E_CUT;
    }
}

static void bass_cmd(char c, char which)
{
    if (c == 'T' || c == 'O') {
        bass_kind = which == '5' ? 1 : which == '3' ? 2 : 0;
        if (bsv.st != E_OFF)
            midi(0x82, bsv.note, 0);
        bsv.note = (uint8_t)bass_note();
        bsv.inc = note_inc(bsv.note);
        bsv.v = 1.0f;
        bsv.st = c == 'T' ? E_SUS : E_BDEC;
        midi(0x92, bsv.note, 100);
    } else if ((c == 'R' || c == 'N') && bsv.st != E_OFF) {
        bsv.st = c == 'R' ? E_REL : E_CUT;
    }
}

/* the chord changed: what sounds moves to it (the OM's chord and bass follow the buttons) */
static void retune(void)
{
    int i;
    om_voicing(root, type, par[P_TRANSPOSE], par[P_OCTAVE], harp_n, chord_n, &bass_n);
    for (i = 0; i < OM_NSTR; i++)                 /* what each string plays when plucked next */
        om_str_note[i] = harp_n[i];
    for (i = 0; i < OM_NCHORD; i++)
        if (chv[i].st != E_OFF && chv[i].note != chord_n[i]) {
            midi(0x81, chv[i].note, 0);
            chv[i].note = chord_n[i];
            chv[i].inc = note_inc(chord_n[i]);
            midi(0x91, chord_n[i], 90);
        }
    if (bsv.st != E_OFF && bsv.note != bass_note()) {
        midi(0x82, bsv.note, 0);
        bsv.note = (uint8_t)bass_note();
        bsv.inc = note_inc(bsv.note);
        midi(0x92, bsv.note, 100);
    }
    shim_rate = SHIM_HZ[((root + par[P_TRANSPOSE]) % 12 + 12) % 12] * 360.0f / OM_SR;
}

/* ---- rhythm */
typedef struct {
    uint32_t pos;
    uint8_t on;
} drum_t;
static drum_t drum[OM_NDRUM];
static int rh, rh_next, step, pulse;            /* pulse: MIDI clock, 0..5 within a step */
static float pulse_left, step_len;
static uint32_t ext_age;                         /* render calls since the last clock tick */
static const uint8_t GM_DRUM[OM_NDRUM] = {36, 75, 42, 51, 38};   /* BD, claves, closed hat, ride, snare */

static void tempo_set(void) { step_len = OM_SR * 60.0f / ((float)par[P_TEMPO] * 4.0f); }

static void do_step(void)
{
    const om_rhythm_t *r = &OM_RHYTHM[rh];
    uint8_t d = r->drum[step];
    int i;
    for (i = 0; i < OM_NDRUM; i++)
        if (d >> i & 1u) {
            drum[i].pos = 0;
            drum[i].on = 1;
            midi(0x99, GM_DRUM[i], 100);
            midi(0x89, GM_DRUM[i], 0);
        }
    om_drum_hit |= d;
    if (gate && par[P_ABC]) {
        if (r->bass[step] != '-')
            bass_cmd(r->bass[step], r->note[step]);
        if (r->chord[step] != '-')
            chord_cmd(r->chord[step]);
    }
    om_step = (uint8_t)step;
    if (++step >= r->len) {
        step = 0;
        if (rh_next != rh) {                     /* a new rhythm starts on the bar */
            rh = rh_next;
            om_steps = OM_RHYTHM[rh].len;
        }
    }
}

/* ---- reverb: Dattorro's plate (J. Dattorro, "Effect Design, Part 1", JAES 1997): a predelay, four
 * input diffusers, then a figure-eight tank of two halves, each a modulated allpass, a delay, a damping
 * lowpass, an allpass and a delay, feeding the other half. Stereo out from seven taps a side. The
 * paper's lengths are for 29761 Hz; DS() scales them to 44.1 kHz. */
#define DS(n) (((n) * 14818 + 5000) / 10000)          /* 44100 / 29761 */
#define PRE_N 442                                    /* 10 ms */
#define EXC DS(16)                                   /* the tank allpasses' modulation, samples */
enum { L_PRE, L_IN1, L_IN2, L_IN3, L_IN4, L_APL, L_D1L, L_AP2L, L_D2L, L_APR, L_D1R, L_AP2R, L_D2R, L_N };
static const int DL_LEN[L_N] = {PRE_N, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC + 2, DS(4453), DS(1800),
                                DS(3720), DS(908) + EXC + 2, DS(4217), DS(2656), DS(3163)};
#define DL_TOTAL (PRE_N + DS(142) + DS(107) + DS(379) + DS(277) + DS(672) + DS(4453) + DS(1800) + DS(3720) + DS(908) + \
                  DS(4217) + DS(2656) + DS(3163) + 2 * (EXC + 2))
static float pl_buf[DL_TOTAL] OM_POOL;
typedef struct {
    float *b;
    int n, i;
} dl_t;
static dl_t dl[L_N];
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.5f, pl_dd2 = 0.5f, lfo_s, lfo_c = 1.0f;

static inline float tap(const dl_t *d, int k)          /* the value written k samples ago, 1..n */
{
    int j = d->i - k;
    return d->b[j < 0 ? j + d->n : j];
}
static inline void push(dl_t *d, float x)
{
    d->b[d->i] = x;
    if (++d->i >= d->n)
        d->i = 0;
}
static inline float allpass(dl_t *d, int len, float x, float g)
{
    float z = tap(d, len), v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}
static inline float allpass_mod(dl_t *d, float len, float x, float g)   /* a fractional, moving length */
{
    int k = (int)len;
    float f = len - (float)k, z = tap(d, k) + (tap(d, k + 1) - tap(d, k)) * f, v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}

static inline void reverb(float in, float *ol, float *or_)
{
    float x, a, b, fb_l = tap(&dl[L_D2R], DS(3163)), fb_r = tap(&dl[L_D2L], DS(3720));
    push(&dl[L_PRE], in);
    x = tap(&dl[L_PRE], PRE_N);
    pl_bw += 0.7f * (x - pl_bw);                     /* input bandwidth */
    x = allpass(&dl[L_IN1], DS(142), pl_bw, 0.75f);
    x = allpass(&dl[L_IN2], DS(107), x, 0.75f);
    x = allpass(&dl[L_IN3], DS(379), x, 0.625f);
    x = allpass(&dl[L_IN4], DS(277), x, 0.625f);
    {   /* the modulation: ~0.9 Hz, sine and cosine for the two halves */
        float s = lfo_s + 0.000128f * lfo_c, c = lfo_c - 0.000128f * s;
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(&dl[L_APL], (float)DS(672) + (float)EXC * lfo_s, x + pl_decay * fb_l, -0.7f);
    push(&dl[L_D1L], a);
    b = tap(&dl[L_D1L], DS(4453));
    pl_damp_l += 0.62f * (b - pl_damp_l);
    b = allpass(&dl[L_AP2L], DS(1800), pl_damp_l * pl_decay, pl_dd2);
    push(&dl[L_D2L], b);
    a = allpass_mod(&dl[L_APR], (float)DS(908) + (float)EXC * lfo_c, x + pl_decay * fb_r, -0.7f);
    push(&dl[L_D1R], a);
    b = tap(&dl[L_D1R], DS(4217));
    pl_damp_r += 0.62f * (b - pl_damp_r);
    b = allpass(&dl[L_AP2R], DS(2656), pl_damp_r * pl_decay, pl_dd2);
    push(&dl[L_D2R], b);
    *ol = 0.6f * (tap(&dl[L_D1R], DS(266)) + tap(&dl[L_D1R], DS(2974)) - tap(&dl[L_AP2R], DS(1913)) +
                  tap(&dl[L_D2R], DS(1996)) - tap(&dl[L_D1L], DS(1990)) - tap(&dl[L_AP2L], DS(187)) -
                  tap(&dl[L_D2L], DS(1066)));
    *or_ = 0.6f * (tap(&dl[L_D1L], DS(353)) + tap(&dl[L_D1L], DS(3627)) - tap(&dl[L_AP2L], DS(1228)) +
                   tap(&dl[L_D2L], DS(2673)) - tap(&dl[L_D1R], DS(2111)) - tap(&dl[L_AP2R], DS(335)) -
                   tap(&dl[L_D2R], DS(121)));
}

static void plate_init(void)
{
    float *p = pl_buf;
    int k, i;
    for (k = 0; k < L_N; k++) {
        dl[k].b = p;
        dl[k].n = DL_LEN[k];
        dl[k].i = 0;
        p += DL_LEN[k];
    }
    for (i = 0; i < DL_TOTAL; i++)
        pl_buf[i] = 0.0f;
    pl_bw = pl_damp_l = pl_damp_r = 0.0f;
    lfo_s = 0.0f;
    lfo_c = 1.0f;
}

/* ---- parameters */
static void apply(int p, int v)
{
    const om_param_t *pi = &PARAMS[p];
    int i;
    if (v < pi->lo)
        v = pi->lo;
    if (v > pi->hi)
        v = pi->hi;
    par[p] = (int16_t)v;
    switch (p) {
    case P_VOICE1: v1 = (float)v * 0.01f; break;
    case P_VOICE2: v2 = (float)v * 0.01f; break;
    case P_SUSTAIN: sus_rate = (1.0f / OM_SR) / (0.366f + 2.734f * (float)v * 0.01f); break;
    case P_CHORD: cvol = (float)v * 0.01f; break;
    case P_RHYVOL: rvol = (float)v * 0.01f; break;
    case P_REVERB: rsend = (float)v * 0.01f * (float)v * 0.01f; break;
    case P_CREV: csend = (float)v * 0.01f * (float)v * 0.01f; break;
    case P_SPACE:                                 /* the plate's decay: a small room to a long hall */
        pl_decay = 0.35f + 0.55f * (float)v * 0.01f;
        pl_dd2 = fm_clampf(pl_decay + 0.15f, 0.25f, 0.5f);
        break;
    case P_TEMPO: tempo_set(); break;
    case P_RHYTHM:
        rh_next = v;
        if (!om_playing) {
            rh = v;
            om_steps = OM_RHYTHM[rh].len;
        }
        break;
    case P_TUNE:
        tunefac = fm_exp2f((float)v * (1.0f / 1200.0f));
        retune();
        break;
    case P_TRANSPOSE: case P_OCTAVE: retune(); break;
    case P_WIDTH:
        for (i = 0; i < OM_NSTR; i++) {           /* low strings left, high right */
            float p0 = ((float)i / 15.0f * 2.0f - 1.0f) * (float)v * 0.01f, a = (p0 + 1.0f) * (FM_PI / 4.0f);
            pan_l[i] = fm_cosf(a);
            pan_r[i] = fm_sinf(a);
        }
        break;
    case P_MIDI:
        if (!v)
            om_midi_out(0xB0, 123, 0);            /* all notes off, as it goes quiet */
        break;
    default: break;
    }
}

static void quiet(void)
{
    int i;
    for (i = 0; i < OM_NSTR; i++)
        str[i].on = 0;
    for (i = 0; i < OM_NCHORD; i++)
        chv[i].st = E_OFF;
    bsv.st = E_OFF;
    for (i = 0; i < OM_NDRUM; i++)
        drum[i].on = 0;
    for (i = 0; i < 3; i++)
        om_midi_out(0xB0 | (uint32_t)i, 123, 0);
}

void om_init(void)
{
    int i;
    for (i = 0; i < 360; i++) {
        float s = fm_sinf((float)(i + 125) * (FM_PI / 180.0f));
        sin3[i] = s == 0.0f ? 0.0f : (s > 0.0f ? 1.0f : -1.0f) * fm_powf(fm_fabsf(s), 0.6f);
    }
    plate_init();
    q_r = q_w = 0;
    root = 0;
    type = CH_MAJ;
    for (i = 0; i < P_NPARAMS; i++)
        apply(i, PARAMS[i].def);
    retune();
}

/* the rhythm's clock: a step every 6 pulses. From its own tempo (pulse_left) or from MIDI clock in */
static void clock_pulse(void)
{
    if (pulse == 0)
        do_step();
    if (!om_ext)
        midi(0xF8, 0, 0);
    if (++pulse >= 6)
        pulse = 0;
}

static void play_start(int from_top)
{
    if (from_top) {
        rh = rh_next;
        om_steps = OM_RHYTHM[rh].len;
        step = 0;
        pulse = 0;
    }
    pulse_left = 0.0f;
    om_playing = 1;
    if (!om_ext)
        midi(from_top ? 0xFA : 0xFB, 0, 0);
}

static void play_stop(void)
{
    om_playing = 0;
    if (!om_ext)
        midi(0xFC, 0, 0);
    if (!gate) {
        chord_cmd('R');
        bass_cmd('R', 'R');
    }
}

static void drain(void)
{
    while (q_r != q_w) {
        uint32_t c = q[q_r % QN];
        int a = (int)(c >> 16 & 0xFFu), b = (int)(int16_t)(c & 0xFFFFu);
        BARRIER();
        q_r++;
        switch (c >> 24) {
        case C_SET:
            if (a < P_NPARAMS)
                apply(a, b);
            break;
        case C_CHORD:
            root = a % 12;
            type = b < CH_NTYPES ? b : 0;
            retune();
            break;
        case C_GATE:
            gate = a;
            if (!(om_playing && par[P_ABC])) {    /* by hand: the chord sounds while the pad is down */
                chord_cmd(a ? 'T' : 'R');
                bass_kind = 0;
                bass_cmd(a ? 'T' : 'R', 'R');
            } else if (!a) {
                chord_cmd('R');
                bass_cmd('R', 'R');
            }
            break;
        case C_STRUM:
            if (a < OM_NSTR)
                strum(a);
            break;
        case C_PLAY:
            if (a && !om_playing)
                play_start(1);
            else if (!a && om_playing)
                play_stop();
            break;
        case C_CLOCK:
            om_ext = 1;                             /* a clock message: someone else leads */
            ext_age = 0;
            if (a == OM_CLK_TICK) {
                if (om_playing)
                    clock_pulse();
            } else if (a == OM_CLK_START) {
                play_start(1);
            } else if (a == OM_CLK_CONTINUE) {
                if (!om_playing)
                    play_start(0);
            } else if (om_playing) {
                play_stop();
            }
            break;
        case C_PANIC:
            quiet();
            break;
        }
    }
}

/* ------------------------------------------------------------- render --- */
#define BLK 256
static float mixl[BLK], mixr[BLK], send[BLK];

static void render_strings(uint32_t n)
{
    const float g1 = v1 * (1.0f - 0.5f * v2) * 1.2f,
                g2 = v2 * (1.0f - 0.5f * v1) * 0.85f * 1.2f;
    int s;
    for (s = 0; s < OM_NSTR; s++) {
        str_t *v = &str[s];
        float pos = v->pos, y1 = v->y1, y2 = v->y2, e = v->v, g = v->g, c = v->c, m = v->m;
        const float inc = v->inc, gl = pan_l[s], gr = pan_r[s];
        uint32_t j;
        if (!v->on) {
            om_str_level[s] = 0.0f;
            continue;
        }
        for (j = 0; j < n; j++) {
            float x1, x2, o;
            if (!(j & 15u)) {                       /* every 16 samples: the filter and the shimmer */
                float c96 = (0.15f + 0.85f * e) * v->k;
                int ph = (int)shim_ph + v->shim_off;
                c = 1.0f - fm_exp2f(2.1768707f * fm_log2f(1.0f - c96));   /* 96 kHz coefficient at 44.1 */
                m = (0.61f + 0.61f * sin3[ph % 360]) * v->modamt;
            }
            x1 = wave(OM_WAVE_HARP_BASE, pos) + wave(OM_WAVE_HARP_MOD, pos) * m;
            x2 = wave(OM_WAVE_HARP, pos);
            y1 += c * (x1 - y1);
            y2 += c * (x2 - y2);
            g += 0.05f * (e - g);
            o = (y1 * g1 + y2 * g2) * g;
            mixl[j] += o * gl;
            mixr[j] += o * gr;
            send[j] += o * rsend;
            pos = adv(pos, inc);
            e -= sus_rate * (0.34f + 8.5f * e * e * e);
            if (e <= 0.0f) {
                e = 0.0f;
                if (g < 1e-4f) {
                    v->on = 0;
                    midi(0x80, v->note, 0);
                    break;
                }
            }
        }
        v->pos = pos;
        v->y1 = fm_flush(y1);
        v->y2 = fm_flush(y2);
        v->v = e;
        v->g = g;
        v->c = c;
        v->m = m;
        om_str_level[s] = v->on ? e : 0.0f;
    }
    shim_ph += shim_rate * (float)n;
    while (shim_ph >= 360.0f)
        shim_ph -= 360.0f;
}

static float env_step(cv_t *v, int bass)
{
    switch (v->st) {
    case E_DEC:
        v->v -= (1.0f / OM_SR) / 0.333f;
        if (v->v <= 0.95f) {
            v->v = 0.95f;
            v->st = E_SUS;
        }
        break;
    case E_BDEC:
        v->v -= (1.0f / OM_SR) / 1.222f;
        if (v->v <= 0.95f) {
            v->v = 0.95f;
            v->st = E_REL;
        }
        break;
    case E_REL:
        v->v -= bass ? (1.0f / OM_SR) / 0.333f * (0.5f + v->v) : (1.0f / OM_SR) / 0.18f;
        break;
    case E_CUT:
        v->v -= (1.0f / OM_SR) / 0.004f;
        break;
    default:
        break;
    }
    if (v->v <= 0.0f) {
        v->v = 0.0f;
        if (v->g < 1e-4f) {
            v->st = E_OFF;
            midi(bass ? 0x82 : 0x81, v->note, 0);
        }
    }
    return v->v;
}

static void render_chord(uint32_t n)
{
    int i;
    uint32_t j;
    const float gc = 0.4f * cvol * 0.7071f, gb = cvol * 0.7071f;
    float lvl = 0.0f;
    for (i = 0; i < OM_NCHORD; i++) {
        cv_t *v = &chv[i];
        if (v->st == E_OFF)
            continue;
        for (j = 0; j < n && v->st != E_OFF; j++) {
            float e = env_step(v, 0), o;
            v->g += 0.05f * (e - v->g);
            o = wave(OM_WAVE_CHORD, v->pos) * v->g * gc;
            mixl[j] += o;
            mixr[j] += o;
            send[j] += o * csend;
            v->pos = adv(v->pos, v->inc);
        }
        lvl = fm_maxf(lvl, v->v);
    }
    om_chord_level = lvl;
    if (bsv.st != E_OFF) {
        cv_t *v = &bsv;
        for (j = 0; j < n && v->st != E_OFF; j++) {
            float e = env_step(v, 1), o;
            v->g += 0.05f * (e - v->g);
            o = (wave(OM_WAVE_BASS_BASE, v->pos) + wave(OM_WAVE_BASS_MOD, v->pos) * (1.0f - e * e)) * v->g * gb;
            mixl[j] += o;
            mixr[j] += o;
            send[j] += o * csend * 0.5f;               /* a little: a wet bass muddies */
            v->pos = adv(v->pos, v->inc);
        }
    }
    om_bass_level = bsv.st != E_OFF ? bsv.v : 0.0f;
}

static void render_drums(uint32_t n)
{
    int i;
    uint32_t j;
    const float g = rvol * (1.0f / 32768.0f) * 0.7071f;
    for (i = 0; i < OM_NDRUM; i++) {
        drum_t *d = &drum[i];
        const int16_t *w = OM_DRUM[i];
        uint32_t len = OM_DRUM_LEN[i];
        if (!d->on)
            continue;
        for (j = 0; j < n; j++) {
            float o;
            if (d->pos >= len) {
                d->on = 0;
                break;
            }
            o = (float)w[d->pos++] * g;
            mixl[j] += o;
            mixr[j] += o;
        }
    }
}

static void render_chunk(int32_t *out, uint32_t n, float gain)
{
    uint32_t j;
    for (j = 0; j < n; j++)
        mixl[j] = mixr[j] = send[j] = 0.0f;
    render_strings(n);
    render_chord(n);
    render_drums(n);
    for (j = 0; j < n; j++) {
        float wl, wr, l, r;
        reverb(send[j] * 0.5f, &wl, &wr);
        l = (mixl[j] + wl) * 0.42f * gain;
        r = (mixr[j] + wr) * 0.42f * gain;
        out[2 * j] = (int32_t)(fm_tanhf(l) * 8300000.0f);
        out[2 * j + 1] = (int32_t)(fm_tanhf(r) * 8300000.0f);
    }
}

void om_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float gain = (float)gain_q12 * (1.0f / 4096.0f) * 2.0f;
    drain();
    if (om_ext && ++ext_age > 100u)                /* ~0.6 s without a tick: back to its own tempo */
        om_ext = 0;
    while (n) {
        uint32_t k = n > BLK ? BLK : n;
        if (om_playing && !om_ext) {
            if (pulse_left <= 0.0f) {
                clock_pulse();
                pulse_left += step_len * (1.0f / 6.0f);
            }
            if ((float)k > pulse_left)
                k = (uint32_t)pulse_left + 1u;
            if (k > n)
                k = n;
            pulse_left -= (float)k;
        }
        render_chunk(out, k, gain);
        out += 2 * k;
        n -= k;
    }
}
