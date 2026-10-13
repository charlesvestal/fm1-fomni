# FoMni on the M-Vave SMK-37

FoMni also builds for the M-Vave SMK-37, a 37-key controller with the same chip as the FM-1. The FM-1 build
is unchanged; everything for the SMK-37 is behind `BOARD_SMK37`.

Until this project's releases include SMK-37 packages, unofficial builds are at https://genkigenki.github.io/smk37/

## Supported models

- **SMK-37 Pro**: this build (package identity `SMK-37 Pro_800`).
- **Starrykey 37 Play**: uses the SMK-37 Pro build.
- **SMK-37 Elite** and **MKE-P37**: the same hardware under other model names. This branch has no build for them
  yet (no `MODEL=elite` / `MODEL=mke`), and the installer refuses a package made for another model.

## Controls

The FM-1 buttons and knobs keep their FoMni meaning (see the README's Playing section); the SMK-37 adds the rest.

| SMK-37 | FoMni |
| --- | --- |
| 16 pads | the strum plate: the 16 strings, with velocity (strings 1-8 on the top row, 9-16 on the bottom row). A string's pad lights while it rings; at rest the chord's roots glow |
| Keys F3..G5, keybed HARP (default) | as on the FM-1: the white keys pluck the strings (with velocity), the black keys are the 11 chord buttons |
| Outer keys, keybed HARP | held, they pick the chord type: C3 minor, D3 7th (both: m7), E3 maj7, C#3 dim, D#3 aug, A5 sus4, B5 add9, C6 m7 |
| Whole keybed, keybed CHORDS | chord buttons: the key's note is the root; C3..B3 major, C4..B4 minor, C5..B5 7th (CHORD / NOTE REPEAT held add minor / 7th); C6 lets the chord go. The strings are on the pads only |
| KNOB BANK | switches the keybed between HARP and CHORDS (lit on CHORDS) |
| K1 | MASTER (volume) |
| K2 / K3 / K4 | SELECT / ALGORITHM / PRESETS |
| K5..K8 | KNOB 1..4 |
| FX, SCALE, CHORD, NOTE REPEAT, PARA, GLOBE | FX, SEL, ENV (minor), LFO (7th), EDIT, GLO |
| PATCH / BT | HOME / SAVE |
| ARP / SEQ | ARP / SEQ |
| SEQ PLAY / SEQ REC | PLAY / REC |
| OCTAVE- / OCTAVE+ | OCT- / OCT+ |
| < / > | the previous / next page |
| FADER BANK | switches the faders between bank A and bank B (lit on B) |
| Faders 1..4, bank A | Voice 1, Voice 2, Chord, Drums |
| Faders 1..4, bank B | Sustain, Reverb, Chord reverb, Space |
| Pitch wheel | pitch bend, +-2 semitones |
| Mod wheel | vibrato (strings, chord and bass) |
| OCTAVE- + OCTAVE+ held at power-on | HARDWARE CALIBRATION: teach the buttons and knobs again |

A fader takes over only once it moves (also after a bank change).

The SMK-37's TRS jack is a MIDI output and this build does not use it; there is no TRS MIDI IN. MIDI works over USB.

## Build

See [BUILDING.md](BUILDING.md) for the toolchain.

```sh
BOARD=smk37 ./build.sh     # build/fomni37.fwsc
```

Without `BOARD`, the FM-1 build is unchanged.

## Install

Connect the keyboard by USB and run:

```sh
python3 tools/fm1_install.py build/fomni37.fwsc
```

`fm1_install.py` needs `mido` and `python-rtmidi` (`pip3 install mido python-rtmidi`). It checks that the package
is for the connected model.

## Going back to the stock firmware

Install M-Vave's firmware package for your model with M-Vave's updater, M-UPGRADE. It works directly, because
the installed package keeps the keyboard's model name.

## Safety

- Back up first: have M-Vave's stock package for your model and M-UPGRADE ready before you install.
- Keep the USB cable connected until the install has finished and the keyboard has restarted.
- The first start clears the stock firmware's settings, and any other firmware's data, in the shared data area.
- The firmware never writes the keyboard's calibration, Bluetooth pairing or key areas; the flash map refuses
  them at compile time and again in the flash driver.
- Installing other firmware is at your own risk.
