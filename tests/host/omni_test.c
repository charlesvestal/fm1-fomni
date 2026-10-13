/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument (dsp/omni.c) on its own: the strum plate's voicing against the OM-108 owner's
 * manual (p.49), the chord and bass registers, envelopes that end, the rhythm's timing, and an output
 * that stays finite and inside full scale with everything playing at once. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "omni.h"

static int bad, midi_n;
void om_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    (void)st;
    (void)d1;
    (void)d2;
    midi_n++;
}

static void check(const char *what, int ok)
{
    printf("%-58s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static int32_t buf[2 * 256];
static float peak;
static int finite_ok = 1;
static void run(int blocks)
{
    int b, i;
    for (b = 0; b < blocks; b++) {
        om_render(buf, 256, 4096);
        for (i = 0; i < 512; i++) {
            float x = (float)buf[i] / 8388608.0f;
            if (!(x == x))
                finite_ok = 0;
            if (fabsf(x) > peak)
                peak = fabsf(x);
        }
    }
}

int main(void)
{
    uint8_t h[OM_NSTR], c[OM_NCHORD], bass;
    /* the manual's C major, zones 1..13: C4 E4 G3 | C5 E5 G4 | C6 E6 G5 | C7 E7 G6 | C8. OMNI's 16 keys
     * add a group below (octave 0), so the manual's zones are OMNI's strings 3..15 */
    static const uint8_t C_OM[13] = {60, 64, 55, 72, 76, 67, 84, 88, 79, 96, 100, 91, 108};
    om_voicing(0, CH_MAJ, 0, 0, h, c, &bass);
    check("C major: strings 3..15 are the OM-108's 13 zones", !memcmp(h + 3, C_OM, 13));
    check("C major: strings 0..2 are the group below (C3 E3 G2)", h[0] == 48 && h[1] == 52 && h[2] == 43);
    check("C major: chord C4 E4 G3 (the F#3..F4 window), bass C2", c[0] == 60 && c[1] == 64 && c[2] == 55 && bass == 36);
    om_voicing(7, CH_MAJ, 0, 0, h, c, &bass);
    check("G major: zone 1 is G3 B3 D4 (manual), bass G2", c[0] == 55 && c[1] == 59 && c[2] == 62 && bass == 43);
    om_voicing(0, CH_7, 0, 0, h, c, &bass);
    check("C7 is a triad without its fifth: C E Bb", c[0] == 60 && c[1] == 64 && c[2] == 58);
    om_voicing(0, CH_MAJ, 2, 1, h, c, &bass);
    check("transpose +2, octave +1: D, a group up", h[0] == 62 && c[0] == 62 && bass == 38);
    {
        int i, ok = 1;
        for (i = 0; i < 12 && ok; i++) {
            int t;
            for (t = 0; t < CH_NTYPES; t++) {
                int k;
                om_voicing(i, t, 0, 0, h, c, &bass);
                for (k = 0; k < OM_NSTR; k++)
                    ok &= h[k] >= 42 && h[k] <= 113;
                for (k = 0; k < OM_NCHORD; k++)
                    ok &= c[k] >= 54 && c[k] <= 65;
                ok &= bass >= 36 && bass <= 47;
            }
        }
        check("every chord: strings F#2..F8, chord F#3..F4, bass C2..B2", ok);
    }

    om_init();
    om_chord(0, CH_MAJ);
    om_strum(0);
    run(2);
    check("a pluck sounds", om_str_level[0] > 0.5f);
    run(44100 / 256 * 8);
    check("a pluck ends (sustain 50: within 8 s)", om_str_level[0] == 0.0f);
    om_set(P_SUSTAIN, 0);
    om_strum(5);
    run(44100 / 256);
    check("sustain 0: gone within 1 s", om_str_level[5] == 0.0f);

    om_gate(1);
    run(10);
    check("a chord button held: the chord and its bass sound", om_chord_level > 0.9f && om_bass_level > 0.9f);
    om_gate(0);
    run(44100 / 256);
    check("let go: both end within 1 s", om_chord_level == 0.0f && om_bass_level == 0.0f);

    {   /* the rhythm: Rock 1 at 120 BPM is a 16th every 5512.5 samples; 32 steps in 4 s */
        int n = 0, last = -1, b;
        om_set(P_TEMPO, 120);
        om_set(P_RHYTHM, 0);
        om_play(1);
        for (b = 0; b < 4 * 44100 / 256; b++) {
            run(1);
            if (om_step != last) {
                n++;
                last = om_step;
            }
        }
        check("Rock 1 at 120: 32 steps (+-1) in 4 s", n >= 31 && n <= 33);
        om_play(0);
        run(2);
        check("stop", !om_playing);
    }
    {   /* everything at once, levels up: finite and inside full scale */
        int i, s;
        om_set(P_VOICE1, 100);
        om_set(P_VOICE2, 100);
        om_set(P_CHORD, 100);
        om_set(P_RHYVOL, 100);
        om_set(P_REVERB, 100);
        om_set(P_SPACE, 100);
        om_set(P_SUSTAIN, 100);
        om_gate(1);
        om_play(1);
        peak = 0.0f;
        for (i = 0; i < 8; i++)
            for (s = 0; s < OM_NSTR; s++) {
                om_strum(s);
                run(1);
            }
        run(44100 / 256 * 3);
        check("all at full: finite output", finite_ok);
        check("all at full: below full scale (the soft clip)", peak < 1.0f && peak > 0.3f);
        printf("  (peak %.2f)\n", (double)peak);
    }
    {   /* the plate: a tail that ends at the longest Space, and stays finite at full send */
        int b, i;
        double e0 = 0.0, e = 0.0;
        om_play(0);
        om_gate(0);
        run(1);                                        /* (om_init empties the queue) */
        om_init();
        om_set(P_REVERB, 100);
        om_set(P_SPACE, 100);
        om_set(P_SUSTAIN, 0);
        om_strum(8);
        for (b = 0; b < 44100 * 20 / 256; b++) {
            om_render(buf, 256, 4096);
            e = 0.0;
            for (i = 0; i < 512; i++)
                e += (double)buf[i] * (double)buf[i];
            if (b == 60)
                e0 = e;
        }
        check("plate at Space 100: the tail is 60 dB down within 20 s", e < e0 * 1e-6);
    }
    check("MIDI out sent notes", midi_n > 100);
    om_set(P_MIDI, 0);
    run(1);
    midi_n = 0;
    om_strum(3);
    run(2);
    check("MIDI out off: nothing sent", midi_n == 0);
    printf("%s\n", bad ? "OMNI TEST FAILED" : "omni test passed");
    return bad != 0;
}
