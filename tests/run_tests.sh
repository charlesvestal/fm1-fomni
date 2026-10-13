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
# The SMK-37 build: the instrument as it compiles it (velocity and wheel hooks in; same results)
run omni-smk37 sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DOM_HOST -DBOARD_SMK37=1 \
    -Ifirmware/src/dsp -o $OUT/omni_test_smk tests/host/omni_test.c firmware/src/dsp/omni.c && $OUT/omni_test_smk"
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
    # The SMK-37 build (BOARD_SMK37): the flash map and store owner, the package layout, the OTA staging and
    # the loader windows, on SMK-layout packages made here from the FM-1 binaries (layout and protocol under test)
    run smk37-flash sh -c "$CC -O2 -Wall -DBOARD_SMK37=1 -Ifirmware/hal -o $OUT/smk_flash_test tests/smk_flash_test.c && \
        $OUT/smk_flash_test"
    if [ -z "${AC79_SDK:-}" ]; then
        echo "  skip smk37 package / update path (needs AC79_SDK, as the build)"
    else
    run smk37-package sh -c "python3 tools/fm1pkg_make.py --board smk37 build/omni.bin build/loader/ota.bin \
        $OUT/smk_new.fwsc >/dev/null && python3 tests/smk_pkg_test.py $OUT/smk_new.fwsc"
    run smk37-ota-entry sh -c "$CC -DBOARD_SMK37=1 -o $OUT/ota_test_smk tests/ota_test.c && \
        $OUT/ota_test_smk $OUT/smk_new.fwsc"
    for m in 1 2; do                     # the MODEL=elite / mke builds of ota.c
        run smk37-ota-entry-model$m sh -c "$CC -DBOARD_SMK37=1 -DT_MODEL=$m -o $OUT/ota_test_smk_m$m tests/ota_test.c && \
            $OUT/ota_test_smk_m$m $OUT/smk_new.fwsc"
    done
    [ -f build/fomni-smk37-elite.fwsc ] && run smk37-build-package-elite python3 tests/smk_pkg_test.py build/fomni-smk37-elite.fwsc "SMK-37 Elite_800"
    [ -f build/fomni-smk37-mke.fwsc ] && run smk37-build-package-mke python3 tests/smk_pkg_test.py build/fomni-smk37-mke.fwsc "MKE-P37_800"
    run smk37-update-loader sh -c "head -c 100000 build/omni.bin > $OUT/old_app.bin && \
        python3 tools/fm1pkg_make.py --board smk37 $OUT/old_app.bin build/loader/ota.bin $OUT/smk_old.fwsc >/dev/null && \
        $CC -DBOARD_SMK37=1 -o $OUT/ldr_test_smk tests/ldr_test.c && $OUT/ldr_test_smk $OUT/smk_old.fwsc $OUT/smk_new.fwsc"
    fi
else
    echo "  skip update-path tests (no build/omni.fwsc: run ./build.sh)"
fi
run host-build sh host/build_host.sh
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
