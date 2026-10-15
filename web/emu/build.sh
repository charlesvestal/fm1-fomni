#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# OMNI in the browser: the host simulator compiled to WebAssembly (Emscripten), plus its page.
#   [OM_VERSION=0.1] web/emu/build.sh  ->  build/emu/{index.html, worklet.js, omni.wasm, reader/}
# Same sources and flags as host/build_host.sh (-ffp-contract=off, like the device).
set -e
cd "$(dirname "$0")/../.."
mkdir -p build/gen build/emu
[ -f build/gen/felucca_font.h ] || python3 tools/gen_font.py build/gen/felucca_font.h >/dev/null
python3 tools/gen_words.py firmware/words.tsv web/words build/gen/ann_words.h >/dev/null
emcc -O2 -ffp-contract=off -std=gnu99 -Wall -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable \
    -DOM_HOST -DOM_WEB "-DOM_VERSION=\"$(printf %s "${OM_VERSION:-DEV}" | tr a-z A-Z)\"" -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
    -o build/emu/omni.wasm web/emu/omni_web.c firmware/src/dsp/omni.c
cp web/emu/index.html web/emu/worklet.js build/emu/
# the screen in words: fm1-reader's words.js and voice (tts.js, eSpeak NG; GPL-3.0), taken from a checkout of
# github.com/charlesvestal/fm1-reader next to this repo (or FM1_READER=its web/ folder) when the site is built
R="${FM1_READER:-../fm1-reader/web}"
rm -rf build/emu/reader
if [ -f "$R/words.js" ] && [ -f "$R/espeak/espeak.wasm" ]; then
    mkdir -p build/emu/reader/espeak
    cp "$R/words.js" "$R/tts.js" "$R/tts_worker.js" build/emu/reader/
    cp "$R/espeak/espeak.js" "$R/espeak/espeak.wasm" "$R/espeak/LICENSE" "$R/espeak/README.md" build/emu/reader/espeak/
    printf 'From fm1-reader (https://github.com/charlesvestal/fm1-reader) web/, commit %s, copied by web/emu/build.sh.\n' \
        "$(git -C "$R" rev-parse --short HEAD 2>/dev/null || echo unknown)" > build/emu/reader/FROM.txt
else
    echo "emu: no fm1-reader at $R: the emulator's screen is read as plain text, without its voice (set FM1_READER)" >&2
fi
echo "emu: build/emu ($(wc -c < build/emu/omni.wasm) B wasm)"
