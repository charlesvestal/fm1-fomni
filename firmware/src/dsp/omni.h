/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument: an OM-84 style chord harp. Three sections, as on the Omnichord:
 *
 *   HARP    16 strings, one per white key. Each plays a tone of the current chord: the tones
 *           folded into an F#..F window, root / third / fifth, five windows up, a root on top
 *           (the OM-84 / OM-108 strumplate, extended down one octave to fill 16 keys).
 *           Two voices, as the OM-84's: VOICE 1 shimmers (its tremolo), VOICE 2 is plain.
 *   CHORD   the chord itself (3 notes, F#3..F4) and its bass (C2..B2), held while a chord pad
 *           is down, or played in time by the rhythm (AUTO BASS SYNC).
 *   RHYTHM  ten OM-84 rhythms: bass drum, claves, hi-hat, cymbal, snare.
 *
 * The waves, envelopes and rhythm patterns follow Chordian (Jan125/pb.chordian, CC0), an OM-84
 * emulator; dsp/om_data.h and dsp/om_drums.h are generated from it (tools/import_chordian.py).
 *
 * Threads: om_render() runs in the audio interrupt. Everything else posts commands (om_cmd), which
 * the render drains at the start of a block; the state below marked (UI) is written by the render
 * and only read elsewhere. Float only, no libm: fastmath.h. */
#pragma once
#include <stdint.h>

#define OM_SR 44100.0f
#define OM_NSTR 16                     /* harp strings: the white keys */
#define OM_NCHORD 3                    /* the OM's chords are triads */

/* chord types: the OM-108's nine (all triads: a 7th drops its fifth, as on the Omnichord) */
enum { CH_MAJ, CH_MIN, CH_7, CH_MAJ7, CH_M7, CH_DIM, CH_AUG, CH_SUS4, CH_ADD9, CH_NTYPES };
extern const char *const OM_TYPE_NAME[CH_NTYPES];      /* "", "m", "7", "M7", "m7", "dim", "aug", "sus4", "add9" */
extern const char *const OM_NOTE_NAME[12];             /* "C", "C#"... */
extern const int8_t OM_TYPE_TONES[CH_NTYPES][3];

/* parameters: value ranges in om_param_info() */
enum {
    P_VOICE1, P_VOICE2, P_SUSTAIN, P_CHORD,          /* harp voice levels, harp sustain, chord + bass level */
    P_RHYTHM, P_TEMPO, P_RHYVOL, P_ABC,               /* rhythm, BPM, rhythm level, auto bass sync */
    P_REVERB, P_SPACE, P_TUNE, P_CREV,                /* the strings' reverb send, reverb size, tune (cents),
                                                       * the chord and bass's reverb send (the drums are dry) */
    P_TRANSPOSE, P_OCTAVE, P_WIDTH, P_MIDI,           /* semitones, harp octave, stereo width, MIDI out */
    P_NPARAMS
};
typedef struct {
    const char *name;
    int16_t lo, hi, def;
} om_param_t;
const om_param_t *om_param_info(int i);
void om_param_text(int i, int v, char *buf);   /* the value as the screen shows it (10 bytes) */

/* the rhythms (dsp/om_data.h): per step, bass and chord commands (T trigger, R release, O one-shot,
 * N cut, '-' none), the bass note (R root, 5 fifth, 3 third) and the drums (bit: BD CL HH CY SD) */
typedef struct {
    const char *name;
    uint8_t len, alt;
    const char *bass, *chord, *note;
    uint8_t drum[32];
} om_rhythm_t;
const om_rhythm_t *om_rhythm(int i);

/* commands (main loop -> render) */
void om_init(void);
void om_set(int param, int value);
void om_chord(int root, int type);             /* the chord the harp, chord and bass play from now on */
void om_gate(int on);                          /* a chord pad held (1) / released (0): the chord section */
void om_strum(int string);                     /* pluck string 0..15 */
void om_play(int on);                          /* the rhythm: start (from its first step) / stop */
void om_panic(void);                           /* everything quiet, now */
/* MIDI clock in (the main loop passes it on): OM_CLK_TICK (F8), START, CONTINUE, STOP. While ticks keep
 * coming (the last within ~0.6 s) they drive the rhythm, 6 a step, and its own tempo stands aside;
 * otherwise the rhythm sends clock (24 a quarter note) and start / stop, with MIDI out on. */
enum { OM_CLK_TICK, OM_CLK_START, OM_CLK_CONTINUE, OM_CLK_STOP };
void om_clock(int msg);

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void om_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* MIDI out, from the render: the app sends it (USB); status, data 1, data 2 */
void om_midi_out(uint32_t st, uint32_t d1, uint32_t d2);

/* state for the screen (UI) */
extern volatile float om_str_level[OM_NSTR];   /* each string's envelope, 0..1 */
extern volatile uint8_t om_str_note[OM_NSTR];  /* the note each string plucks, for the chord now (MIDI) */
extern volatile float om_chord_level, om_bass_level;
extern volatile uint8_t om_playing, om_step, om_steps;
extern volatile uint8_t om_drum_hit;           /* bits: drums hit since the UI last cleared it */
extern volatile uint8_t om_ext;                /* 1: following an external MIDI clock */
/* the notes a chord gives: harp strings (16, with transpose and octave), chord (3) and bass root */
void om_voicing(int root, int type, int transpose, int octave, uint8_t harp[OM_NSTR], uint8_t chord[OM_NCHORD],
                uint8_t *bass);
