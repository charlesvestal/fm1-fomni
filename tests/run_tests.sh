#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# OMNI host tests: the maths, the platform pieces kept from Felucca / X0X, the instrument, and the
# whole firmware app in the simulator (tests/scenarios/*.omni: UI, chords, rhythm, persistence), with
# screenshots and audio in build/scenarios/.
#   tests/run_tests.sh
set -u
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
FAIL=0
OUT=build/host
mkdir -p "$OUT" build/scenarios
run() {
    name="$1"; shift
    if "$@" > "$OUT/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see $OUT/$name.log)"
        tail -15 "$OUT/$name.log" | sed 's/^/       /'
        FAIL=1
    fi
}
run fastmath sh -c "$CC -O2 -ffp-contract=off -Wall -Wextra -Werror -o $OUT/fastmath_test tests/host/fastmath_test.c -lm && $OUT/fastmath_test"
run encoder sh -c "$CC -O2 -w -Ifirmware/hal -o $OUT/encoder_test tests/host/encoder_test.c && $OUT/encoder_test"
run uac sh -c "$CC -O2 -w -Ifirmware/src -Ifirmware/hal -o $OUT/uac_test tests/host/uac_test.c -lm && $OUT/uac_test"
run trs sh -c "$CC -O2 -w -Ifirmware/src -Ifirmware/hal -o $OUT/trs_test tests/host/trs_test.c && $OUT/trs_test"
run omni sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DOM_HOST -Ifirmware/src/dsp \
    -o $OUT/omni_test tests/host/omni_test.c firmware/src/dsp/omni.c && $OUT/omni_test"
run storage sh -c "$CC -O2 -o $OUT/storage_test tests/storage_test.c && $OUT/storage_test"
run flash-ok sh -c "$CC -std=c99 -Wall -Wextra -Werror -o $OUT/flash_ok_test tests/host/flash_ok_test.c && $OUT/flash_ok_test"   # firmware/hal/fm1_flash_ok.h: the flash parts the storage may use
# the M-UPGRADE handshake (firmware/src/ota.c): a release's identity cut to NAME_ and three digits for the plain
# query (M-UPGRADE 3.11), the whole one for the query with body 01; a dev build's; the update loader's, whole
for m in long short loader; do
    case $m in long) f="" ;; short) f="-DTEST_ID='\"FM-1_800\"'" ;; loader) f="-DFELUCCA_LOADER=1" ;; esac
    run "ota-hs-$m" sh -c "$CC -std=c11 -O1 -Wall -Wextra -Werror -Wno-unused-function $f -o $OUT/ota_hs_$m tests/ota_hs_test.c && $OUT/ota_hs_$m"
done
# the web installer's handshake (web/fm1ota.js): a firmware that answers M-UPGRADE 3.11's query short and its whole
# identity to the one with 01, older builds, M-VAVE's firmware, the loader
if command -v node >/dev/null 2>&1; then
    run installer-hs node tests/host/installer_hs_test.mjs
fi
# the update path, against the firmware package (Felucca's tests; needs ./build.sh)
if [ -f build/omni.fwsc ]; then
    run ota-entry sh -c "$CC -o $OUT/ota_test tests/ota_test.c && $OUT/ota_test build/omni.fwsc"
    if [ -z "${AC79_SDK:-}" ]; then
        echo "  skip update-loader (needs AC79_SDK, as the build)"
    else
    run update-loader sh -c "head -c 100000 build/omni.bin > $OUT/old_app.bin && \
        python3 tools/fm1pkg_make.py $OUT/old_app.bin build/loader/ota.bin $OUT/old.fwsc >/dev/null && \
        $CC -o $OUT/ldr_test tests/ldr_test.c && $OUT/ldr_test $OUT/old.fwsc build/omni.fwsc"
    fi
    run installer python3 tests/install_test.py
else
    echo "  skip update-path tests (no build/omni.fwsc: run ./build.sh)"
fi
run host-build sh host/build_host.sh
if command -v node >/dev/null 2>&1; then
    run page-scripts python3 tests/page_script_test.py
fi
if command -v emcc >/dev/null 2>&1; then
    run emu sh -c "sh web/emu/build.sh >/dev/null && node tests/host/emu_test.mjs build/emu/omni.wasm"
fi
for s in tests/scenarios/*.omni; do
    n=$(basename "$s" .omni)
    mkdir -p "build/scenarios/$n"
    run "scenario-$n" build/host/omni_host "$s" "build/scenarios/$n"
done
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
