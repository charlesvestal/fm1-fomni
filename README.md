# FoMni for the M-VAVE FM-1

FoMni turns your M-VAVE FM-1 into a chord harp, inspired by the Suzuki Omnichord. The 16 white
keys are the strum plate: run a finger across them and they ring out the chord. The 11 black keys
are the chord buttons. Under the strings, an organ-like chord and a bass hold the harmony, and the
OM-84's ten rhythms can play them in time (auto bass sync).

It's meant to be simple. There's one screen with four knobs a page, and no patterns to program.

[![FoMni running on the FM-1 (watch on YouTube)](https://img.youtube.com/vi/pahaM6nzucE/maxresdefault.jpg)](https://www.youtube.com/watch?v=pahaM6nzucE)

![FoMni's screen: a C chord strummed](docs/img/screen-strum.png) ![A minor held](docs/img/screen-chord.png) ![The rhythm page](docs/img/screen-rhythm.png)

- **Try it in your browser** (no FM-1 needed): <https://charlesvestal.github.io/fm1-fomni/emu/>
- **Install it** (Chrome or Edge, with the FM-1 plugged in): <https://charlesvestal.github.io/fm1-fomni/install/>
- **Download the firmware file**: [releases](https://github.com/charlesvestal/fm1-fomni/releases)

Install at your own risk, though it's hard to get stuck. The web installer can always put M-VAVE's
firmware back, and an FM-1 that keeps crashing starts in a safe mode.

## Playing

| | |
|---|---|
| White keys | The strum plate. Each key plucks one string of the chord. As on the Omnichord, each group of three keys is root, third and fifth, five groups up, folded into an F#–F window, with one more root on top. |
| Black keys | The chord buttons, F C G, Dm Am, Em G7 E7, D7 Bb and A7 at first. Hold **ENV** for the minor, **LFO** for the 7th, or both for the m7, like the Omnichord's three rows. |
| PLAY | Starts and stops the rhythm. |
| REC | Turns on SYNC START, so the rhythm waits for your first chord. |
| ARP | Turns on CHORD HOLD, so the chord keeps playing after you let go. Press the same chord button again to stop it. |
| OCT− / OCT+ | Move the strum plate down or up an octave. |
| SELECT | Sets the tempo. |
| ALGORITHM | Picks the rhythm. |
| PRESETS | Transposes, from −6 to +6. |
| KNOB 1–4 | Set the four values on the screen. The pages below list them. |
| SAVE | Saves. FoMni also saves by itself a few seconds after a change, once it's quiet. |

### The pages

Each page puts four values on KNOB 1–4. Press EDIT to step through the pages.

- The Play page (**HOME**) has Voice 1, Voice 2, Sustain and Chord level.
- The Rhythm page (**SEQ**) has Rhythm, Tempo, Drums level and Auto bass. With Auto bass on, the bass and chord
  play in time with the rhythm.
- The Sound page (**FX**) has Reverb, how much of the strings go to the reverb; Space, the plate's size; Width; and
  Chord rev, how much of the chord and bass go to the reverb. The drums stay dry.
- On the Chords page (**SEL**), you can change the chord on any black key. Press the key, then turn Root (KNOB 1)
  and Type (KNOB 2). KNOB 3 and 4 set Transpose and Octave.
- The Setup page (**GLO**) has Tune, MIDI out and Lights. With Lights on, the keys light up and the unlit buttons
  glow dimly so you can find them. With Keys, only the keys light. Off turns the lights off.

**Voice 1** is the Omnichord's shimmering harp, and **Voice 2** is its plain one. Mix them, and
set how long the strings ring with **Sustain**.

### MIDI

MIDI comes in over USB and the TRS jack, and goes out over USB. The FM-1's jack is input only.

- Notes coming in on channel 1 pluck the nearest string.
- Play a chord on a keyboard on channel 2, and it becomes the chord, as if you'd pressed a black key.
- When MIDI clock comes in, the rhythm follows it, along with start and stop. The screen shows EXT for the tempo.
- MIDI out is on by default, and you can turn it off on the Setup page. The strings go out on channel 1, the
  chord on 2, the bass on 3 and the drums on 10. When the rhythm runs on its own tempo, FoMni also sends clock,
  start and stop.

The drums on channel 10:

| Sound | Note |
|---|---|
| Bass drum | 36 |
| Snare | 38 |
| Closed hat | 42 |
| Ride | 51 |
| Claves | 75 |

### Screen reader

FoMni-1 tells [FM-1 Screen Reader](https://charlesvestal.github.io/fm1-reader/) what its screen shows, over
USB. You'll hear the page and its knobs, a knob's value as you turn it, the chord you play, play and stop, and
what each button did. Open the page in Chrome with the FM-1 plugged in. It speaks with its own voice, or hands
the text to your computer's screen reader.

## Building and testing

The toolchain is the same as X0X's (see [BUILDING.md](BUILDING.md)):

```
tests/run_tests.sh      # the instrument, the platform pieces, the app in a simulator
./build.sh              # build/omni.fwsc
python3 tools/fm1_install.py build/omni.fwsc     # install it over USB
```

`build/host/omni_host SCRIPT OUTDIR` runs the whole app on a computer from a script
(`tests/scenarios/*.omni`), with audio and screenshots coming out. `web/emu/build.sh` builds the
same code for the browser.

## Credits

The rhythms, waves, drum sounds and envelope shapes come from
[Chordian](https://github.com/Jan125/pb.chordian), Jan125's OM-84 emulator (CC0). The platform
(hardware layer, USB, update loader, storage, installer) is
[Felucca](https://github.com/hugelton/Felucca)'s by Leo Kuroshita (Hügelton Instruments), by way of
[X0X](https://github.com/charlesvestal/fm1-x0x). GPL-3.0-only; see [LICENSING.md](LICENSING.md).

Omnichord is a trademark of Suzuki. FoMni isn't affiliated with or endorsed by Suzuki, M-VAVE or
Hügelton Instruments.

You can also try FoMni in the browser, no flashing needed, in the free community [FM-1 Emulator](https://fm1-emulator.atst.tools/).
