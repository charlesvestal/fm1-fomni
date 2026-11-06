#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make the FoMni-1 site for GitHub Pages (https://charlesvestal.github.io/fm1-fomni/):

  index.html                    what FoMni-1 is, how it plays, and the ways in: try, install, download, source
  img/                          screenshots (docs/img)
  emu/                          FoMni-1 in the browser (build/emu, from web/emu/build.sh), with fm1-reader's
                                words.js and voice in emu/reader/ (the screen in words)
  words/                        the screen reader's words files (web/words, from firmware/words.tsv)
  install/index.html            the web installer (web/omni_installer.html, with fm1pkg.js, fm1ota.js and
                                the package's metadata inlined; Chrome or Edge, Web MIDI)
  firmware/omni-VERSION.fwsc    the package the installer writes; also the download

  tools/make_pages.py build/omni-0.1-beta.fwsc 0.1-beta OUT_DIR

The package must be a release build (./build.sh --release X.Y): its identity, FM-1_8XXYYZZ, is what
the installer checks the download against and what the FM-1 reports after the install."""
import html
import json
import re
import shutil
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SRC / "web"))
from make_site import product_of, strip_module  # noqa: E402  (Felucca's: the package format)

REPO = "https://github.com/charlesvestal/fm1-fomni"

LANDING = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>FoMni-1 · a chord harp for the FM-1</title>
<meta name="description" content="A chord harp for the M-VAVE FM-1, inspired by the Suzuki Omnichord: strum the white keys, pick chords on the black ones.">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow:wght@400;600&family=Barlow+Semi+Condensed:wght@600;700&display=swap">
<style>
/* light: --muted, --accent-ink and --teal darkened just enough for 4.5:1 on the cream; --accent (the bright coral)
   only for the big title */
:root { --ground: #faf4e8; --ink: #48392f; --muted: #6e5c4b; --rule: #e6dccb; --accent: #e0683a; --accent-ink: #b84a1e; --teal: #1b776e;
        --card: #fffaf1; --focus: #1f6fd1; color-scheme: light; }
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) { --ground: #1c1814; --ink: #f2e8d8; --muted: #b3a28e; --rule: #3a322a; --accent: #f28a5a; --accent-ink: #f28a5a; --teal: #4cc3b6; --card: #26201b; --focus: #79adff; color-scheme: dark; }
}
:root[data-theme="dark"] { --ground: #1c1814; --ink: #f2e8d8; --muted: #b3a28e; --rule: #3a322a; --accent: #f28a5a; --accent-ink: #f28a5a; --teal: #4cc3b6; --card: #26201b; --focus: #79adff; color-scheme: dark; }
:focus-visible { outline: 3px solid var(--focus); outline-offset: 2px; }
body { background: var(--ground); color: var(--ink); margin: 0; padding-inline: 16px; padding-block: 48px 64px;
       font: 400 17px/1.6 "Barlow", "Helvetica Neue", Arial, sans-serif; }
main { max-width: 42rem; margin: 0 auto; }
h1 { font: 700 3rem/1 "Barlow Semi Condensed", "Barlow", sans-serif; margin: 0 0 .5rem; color: var(--accent); }
h2 { font: 700 1.4rem "Barlow Semi Condensed", "Barlow", sans-serif; margin: 2.2rem 0 .6rem; }
.lede { font-size: 1.15rem; margin: 0 0 1.6rem; max-width: 36rem; }
.shots { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px; margin: 0 0 1.8rem; }
.shots img { width: 100%; height: auto; border-radius: 14px; image-rendering: pixelated; box-shadow: 0 1px 0 var(--rule), 0 6px 18px rgba(72, 57, 47, .12); }
.status { border: 1px solid var(--rule); background: var(--card); border-radius: 14px; padding: 12px 16px; margin: 0 0 1.6rem; }
.ways { display: grid; gap: 12px; margin: 0 0 1rem; }
.ways a { display: block; text-decoration: none; color: var(--ink); background: var(--card); border: 1px solid var(--rule);
          border-radius: 14px; padding: 14px 18px; }
.ways a:hover, .ways a:focus-visible { border-color: var(--accent-ink); }
.ways strong { font: 600 1.25rem "Barlow Semi Condensed", "Barlow", sans-serif; color: var(--accent-ink); display: block; }
.ways span { color: var(--muted); font-size: .95rem; }
table { border-collapse: collapse; width: 100%; font-size: .95rem; }
th, td { border-top: 1px solid var(--rule); padding: 8px 10px 8px 0; vertical-align: top; text-align: left; }
th { font-weight: 600; white-space: nowrap; color: var(--teal); }
p.small { color: var(--muted); font-size: .9rem; }
a { color: var(--accent-ink); }
footer { max-width: 42rem; margin: 0 auto; }
code { font-size: .9em; }
ul.pages { padding-left: 1.2rem; margin: 0 0 1rem; }
ul.pages li { margin: 0 0 .4rem; }
ul.pages b { color: var(--teal); }
.video { position: relative; aspect-ratio: 16 / 9; max-width: 100%; margin: 0 0 1.4rem; border-radius: 14px; overflow: hidden; background: #000; }
.video iframe { position: absolute; inset: 0; width: 100%; height: 100%; border: 0; }
</style>
</head>
<body>
<main>
<h1>FoMni-1</h1>
<div class="video"><iframe src="https://www.youtube-nocookie.com/embed/pahaM6nzucE" title="FoMni-1 running on the FM-1"
  allow="accelerometer; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe></div>
<p class="lede">FoMni-1 turns your M-VAVE FM-1 into a chord harp, inspired by the Suzuki Omnichord. Pick a chord
on the black keys, run a finger across the white keys, and it rings out. Under the strings, an organ-like chord and a
bass hold the harmony, and the OM-84's ten rhythms can play them in time.</p>
<div class="shots">
  <img src="img/screen-strum.png" width="240" height="240" alt="The Play page: the chord C (C E G), its sixteen strings ringing, the eleven chord buttons F to A7 below, then Voice 1 80, Voice 2 45, Sustain 50, Chord 60">
  <img src="img/screen-chord.png" width="240" height="240" alt="The Play page with A minor held (A C E): its chord button lit, the strings still">
  <img src="img/screen-rhythm.png" width="240" height="240" alt="The Rhythm page: Rock 1 at 116, its drum pattern as rows of dots over the chord buttons, then Rhythm Rock 1, Tempo 116, Drums 70, Auto bass On">
</div>
<div class="status"><strong>Version __VERSION__.</strong> You install and remove it the same way as Felucca and
X0X, and the installer can always put M-VAVE's own firmware back. Install at your own risk.</div>
<nav class="ways" aria-label="Get FoMni-1">
  <a href="emu/"><strong>Try it in the browser</strong><span>Play it right here, with sound: drag across the white keys to strum. It runs the same code as the FM-1, so you don't need one.</span></a>
  <a href="install/"><strong>Install</strong><span>Connect your FM-1 by USB and install from Chrome or Edge. There's nothing to install on your computer.</span></a>
  <a href="firmware/__PKG__"><strong>Download __PKG__</strong><span>For the command-line installer: <code>python3 tools/fm1_install.py __PKG__</code></span></a>
  <a href="__REPO__"><strong>Source</strong><span>On GitHub under GPL-3.0. Built on Felucca by Hügelton Instruments, with sounds and rhythms from Jan125's Chordian.</span></a>
</nav>

<h2 id="playing">Playing</h2>
<table aria-labelledby="playing">
<tr><th scope="row">White keys</th><td>The strum plate. Each key plucks one string of the chord. As on the Omnichord, the keys go root, third and fifth, five times up, folded into an F#–F window, with one more root on top.</td></tr>
<tr><th scope="row">Black keys</th><td>The chord buttons: F C G, Dm Am, Em G7 E7, D7 Bb, A7. Hold ENV for the minor, LFO for the 7th, or both for the m7. To change any of them, use the Chords page (SEL).</td></tr>
<tr><th scope="row">PLAY · REC</th><td>PLAY starts and stops the rhythm. REC turns on sync start, so the rhythm waits for your first chord.</td></tr>
<tr><th scope="row">ARP</th><td>Turns on chord hold, so the chord keeps playing after you let go.</td></tr>
<tr><th scope="row">OCT− · OCT+</th><td>Move the strum plate down or up an octave.</td></tr>
<tr><th scope="row">SELECT</th><td>Sets the tempo.</td></tr>
<tr><th scope="row">ALGORITHM</th><td>Picks the rhythm.</td></tr>
<tr><th scope="row">PRESETS</th><td>Transposes, from −6 to +6.</td></tr>
<tr><th scope="row">KNOB 1–4</th><td>Set the four values on the screen. The pages below list them.</td></tr>
<tr><th scope="row">SAVE</th><td>Saves. FoMni-1 also saves by itself a few seconds after a change, once it's quiet.</td></tr>
</table>

<h2>The pages</h2>
<p>Each page puts four values on KNOB 1–4. Press EDIT to step through the pages.</p>
<ul class="pages">
  <li>The Play page (<b>HOME</b>) has Voice 1, Voice 2, Sustain and Chord level.</li>
  <li>The Rhythm page (<b>SEQ</b>) has Rhythm, Tempo, Drums level and Auto bass. With Auto bass on, the bass and chord play in time with the rhythm.</li>
  <li>The Sound page (<b>FX</b>) has Reverb, how much of the strings go to the reverb; Space, the plate's size; Width; and Chord rev, how much of the chord and bass go to the reverb. The drums stay dry.</li>
  <li>On the Chords page (<b>SEL</b>), you can change the chord on any black key. Press the key, then turn Root (KNOB 1) and Type (KNOB 2). KNOB 3 and 4 set Transpose and Octave.</li>
  <li>The Setup page (<b>GLO</b>) has Tune, MIDI out and Lights. With Lights on, the keys light up and the unlit buttons glow dimly so you can find them. With Keys, only the keys light. Off turns the lights off.</li>
</ul>
<h2>MIDI</h2>
<p>MIDI comes in over USB and the TRS jack, and goes out over USB. The FM-1's jack is input only.</p>
<ul class="pages">
  <li>Notes coming in on channel 1 pluck the nearest string.</li>
  <li>Play a chord on a keyboard on channel 2, and it becomes the chord, as if you'd pressed a black key.</li>
  <li>When MIDI clock comes in, the rhythm follows it, along with start and stop. The screen shows EXT for the tempo.</li>
  <li>MIDI out is on by default, and you can turn it off on the Setup page. The strings go out on channel 1, the chord on 2, the bass on 3 and the drums on 10. When the rhythm runs on its own tempo, FoMni-1 also sends clock, start and stop.</li>
</ul>
<h2>Screen reader</h2>
<p>FoMni-1 can tell a screen reader what its screen shows. Connect the FM-1 by USB and open
<a href="https://charlesvestal.github.io/fm1-reader/">FM-1 Screen Reader</a> in Chrome or Edge. Your own screen reader,
or the page's voice, reads the pages, the knobs as you turn them, and the chords. <a href="emu/">FoMni-1 in the browser</a> speaks its screen the same way.</p>
</main>
<footer>
<p class="small">To go back to the official firmware, use "Back to the stock firmware" in the installer, or
M-VAVE's own updater, M-UPGRADE, from <a href="https://www.m-vave.com/download">m-vave.com/download</a>. M-UPGRADE
3.11 finds the current version of FoMni-1, but not older ones. Omnichord is a trademark of Suzuki; M-VAVE and FM-1 are
trademarks of their owners. FoMni-1 is not affiliated with any of them.</p>
</footer>
</body>
</html>
"""


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_8\d{6}", product):
        raise SystemExit(f"{pkg}: identity {product!r}: make a release build (./build.sh --release X.Y)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no update loader in it")
    name = f"omni-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    if out.exists():
        shutil.rmtree(out)
    for d in ("install", "firmware"):
        (out / d).mkdir(parents=True)
    shutil.copy(pkg, out / "firmware" / name)
    page = (SRC / "web" / "omni_installer.html").read_text(encoding="utf-8")
    lib = strip_module((SRC / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((SRC / "web" / "fm1ota.js").read_text(encoding="utf-8"))
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if page.count(mark) != 1:
            raise SystemExit(f"omni_installer.html must contain {mark} once")
    (out / "install" / "index.html").write_text(page.replace("/*LIB*/", lib).replace("/*META*/", meta), encoding="utf-8")
    shutil.copytree(SRC / "docs" / "img", out / "img")
    (out / "index.html").write_text(LANDING.replace("__VERSION__", html.escape(version)).replace("__PKG__", name)
                                    .replace("__REPO__", REPO), encoding="utf-8")
    emu = SRC / "build" / "emu"
    if not (emu / "omni.wasm").exists():
        raise SystemExit("no build/emu/omni.wasm: run web/emu/build.sh (needs Emscripten)")
    if not (emu / "reader" / "words.js").exists():
        raise SystemExit("no build/emu/reader/: the emulator's screen reader comes from fm1-reader "
                         "(clone github.com/charlesvestal/fm1-reader next to this repo, or set FM1_READER), then web/emu/build.sh")
    shutil.copytree(emu, out / "emu")
    # the screen reader's words files (fm1-reader): every one ever made, for the devices that point at them
    shutil.copytree(SRC / "web" / "words", out / "words")
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html, install/ ({product}), emu/, words/, firmware/{name} ({len(raw)} B)")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
