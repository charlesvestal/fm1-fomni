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
