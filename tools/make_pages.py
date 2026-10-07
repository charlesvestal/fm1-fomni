#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make the FoMni-1 site for GitHub Pages (https://charlesvestal.github.io/fm1-fomni/):

  index.html                    what FoMni-1 is, how it plays, and the ways in: try, install, download, source
  img/                          screenshots (docs/img)
  emu/                          FoMni-1 in the browser (build/emu, from web/emu/build.sh)
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
<title>FoMni-1</title>
<meta name="description" content="A chord harp for the M-VAVE FM-1, inspired by the Suzuki Omnichord: strum the white keys, pick chords on the black ones.">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow:wght@400;600&family=Barlow+Semi+Condensed:wght@600;700&display=swap">
<style>
:root { --ground: #faf4e8; --ink: #48392f; --muted: #8a7764; --rule: #e6dccb; --accent: #e0683a; --teal: #26a69a; --card: #fffaf1; color-scheme: light; }
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) { --ground: #1c1814; --ink: #f2e8d8; --muted: #b3a28e; --rule: #3a322a; --accent: #f28a5a; --teal: #4cc3b6; --card: #26201b; color-scheme: dark; }
}
:root[data-theme="dark"] { --ground: #1c1814; --ink: #f2e8d8; --muted: #b3a28e; --rule: #3a322a; --accent: #f28a5a; --teal: #4cc3b6; --card: #26201b; color-scheme: dark; }
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
.ways a:hover, .ways a:focus-visible { border-color: var(--accent); outline: none; }
.ways strong { font: 600 1.25rem "Barlow Semi Condensed", "Barlow", sans-serif; color: var(--accent); display: block; }
.ways span { color: var(--muted); font-size: .95rem; }
table { border-collapse: collapse; width: 100%; font-size: .95rem; }
td { border-top: 1px solid var(--rule); padding: 8px 10px 8px 0; vertical-align: top; }
td:first-child { font-weight: 600; white-space: nowrap; color: var(--teal); }
p.small { color: var(--muted); font-size: .9rem; }
a { color: var(--accent); }
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
<p class="lede">A chord harp for the M-VAVE FM-1, inspired by the Suzuki Omnichord. Pick a chord on the black
keys, run a finger across the white keys, and it rings out. Behind it are an organ-like chord, a bass, and the
OM-84's ten rhythms, which can play the bass and chord in time.</p>
<div class="shots">
  <img src="img/screen-strum.png" width="240" height="240" alt="FoMni-1's screen: the chord C, sixteen strings ringing, the chord buttons below">
  <img src="img/screen-chord.png" width="240" height="240" alt="FoMni-1's screen: A minor held">
  <img src="img/screen-rhythm.png" width="240" height="240" alt="FoMni-1's rhythm page: the Rock 1 pattern">
</div>
<div class="status"><strong>Version __VERSION__.</strong> It installs and
uninstalls the way Felucca and X0X do, and the installer can put M-VAVE's own firmware back. Installing is at your own risk.</div>
<nav class="ways" aria-label="Get FoMni-1">
  <a href="emu/"><strong>Try it in the browser</strong><span>The same code the FM-1 runs, with sound. Drag across the white keys to strum; no FM-1 needed.</span></a>
  <a href="install/"><strong>Install</strong><span>From Chrome or Edge, with the FM-1 connected by USB. Nothing to install on the computer.</span></a>
  <a href="firmware/__PKG__"><strong>Download __PKG__</strong><span>For the command-line installer: <code>python3 tools/fm1_install.py __PKG__</code></span></a>
  <a href="__REPO__"><strong>Source</strong><span>GitHub, GPL-3.0. Built on Felucca by Hügelton Instruments; sounds and rhythms from Jan125's Chordian.</span></a>
</nav>

<h2>Playing</h2>
<table>
<tr><td>White keys</td><td>The strum plate. Each key plucks a string of the chord: root, third and fifth, folded into an F#–F window, five groups up and a root on top, as on the Omnichord.</td></tr>
<tr><td>Black keys</td><td>The chord buttons: F C G, Dm Am, Em G7 E7, D7 Bb, A7. Hold ENV for the minor, LFO for the 7th, both for the m7. Change any of them on the Chords page (SEL).</td></tr>
<tr><td>PLAY · REC</td><td>The rhythm, on and off. REC is sync start: the rhythm waits for your first chord.</td></tr>
<tr><td>ARP</td><td>Chord hold: the chord keeps going after you let go.</td></tr>
<tr><td>OCT− · OCT+</td><td>Move the strum plate an octave.</td></tr>
<tr><td>SELECT</td><td>Tempo.</td></tr>
<tr><td>ALGORITHM</td><td>Rhythm.</td></tr>
<tr><td>PRESETS</td><td>Transpose (−6 to +6).</td></tr>
<tr><td>KNOB 1–4</td><td>The four values on the screen (see the pages below).</td></tr>
<tr><td>SAVE</td><td>Saves. FoMni-1 also saves by itself a few seconds after a change, once it's quiet.</td></tr>
</table>

<h2>The pages</h2>
<p>Each page puts four values on KNOB 1–4. EDIT steps through the pages.</p>
<ul class="pages">
  <li><b>HOME: Play.</b> Voice 1, Voice 2, Sustain, Chord level.</li>
  <li><b>SEQ: Rhythm.</b> Rhythm, Tempo, Drums level, Auto bass (on: the bass and chord play in time with the rhythm).</li>
  <li><b>FX: Sound.</b> Reverb (the strings' send), Space (the plate's size), Width, Chord rev (the chord and bass's send). The drums stay dry.</li>
  <li><b>SEL: Chords.</b> Change the chord on any black key: press the key, then turn Root (KNOB 1) and Type (KNOB 2). KNOB 3 and 4 are Transpose and Octave.</li>
  <li><b>GLO: Setup.</b> Tune, MIDI out, Key lights.</li>
</ul>
<p class="small">MIDI out: the strings on channel 1, the chord on 2, the bass on 3, the drums on 10. To go back to the official
firmware, use the installer's "Back to the stock firmware", or M-VAVE's updater, M-UPGRADE, from
<a href="https://www.m-vave.com/download">m-vave.com/download</a>. Omnichord is a trademark of Suzuki; M-VAVE and FM-1 are
trademarks of their owners. FoMni-1 is not affiliated with any of them.</p>
</main>
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
    shutil.copytree(emu, out / "emu")
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html, install/ ({product}), emu/, firmware/{name} ({len(raw)} B)")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
