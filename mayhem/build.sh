#!/usr/bin/env bash
#
# mayhem/build.sh — shadPS4 file-format parser fuzz harnesses.
#
# Like upstream's tests/CMakeLists.txt, this compiles only the sources under test plus the
# minimal common/logging support and upstream's tests/stubs/*.cpp, instead of the whole
# emulator (Vulkan/SDL window/guest address space). The upstream sources are compiled with
# $SANITIZER_FLAGS so the fuzzed parsers are instrumented. Header-only externals and
# fmt/spdlog come from /opt/toolchains/deps (pinned to upstream's submodule commits).
#
# Outputs (mayhem/build/, gitignored):
#   fuzz_<t> + fuzz_<t>-standalone  for t in npbind playgo rif psf
#   selftest                         known-answer oracle (normal flags), run by mayhem/test.sh
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
D=/opt/toolchains/deps
FMT=$D/fmt-ec73fb7 SPD=$D/spdlog-b8944a4
INC=(-I"$SRC/src" -I"$SRC"
     -isystem "$FMT/include" -isystem "$SPD/include/spdlog-2"
     -isystem "$D/magic_enum-a413fcc/src/include" -isystem "$D/robin-map-4ec1bf1/src/include"
     -isystem "$D/toml11-a01fe3b/src/include" -isystem "$D/json-55f9368/src/include")
DEFS=(-DSPDLOG_FMT_EXTERNAL -DSPDLOG_NO_EXCEPTIONS -DSPDLOG_DISABLE_GLOBAL_LOGGER
      -DSPDLOG_FUNCTION=__func__ -DBOOST_ASIO_STANDALONE)
LIBS=("$SPD/lib/spdlog-2/libspdlog.a" "$FMT/lib/libfmt.a" -lpthread)

# Upstream sources under test + support (the SETTINGS_TEST_SOURCES set, plus io_file and
# the parsers).
SOURCES=(
  src/core/file_format/psf.cpp
  src/core/file_format/npbind.cpp
  src/core/file_format/playgo_chunk.cpp
  src/core/file_format/rif.cpp
  src/core/emulator_settings.cpp
  src/core/emulator_state.cpp
  src/common/path_util.cpp
  src/common/assert.cpp
  src/common/error.cpp
  src/common/io_file.cpp
  src/common/string_util.cpp
  src/common/logging/classes.cpp
  src/common/logging/log.cpp
  tests/stubs/common_stub.cpp
  tests/stubs/core_stub.cpp
  tests/stubs/signals_stub.cpp
  tests/stubs/file_sys_stub.cpp
  tests/stubs/scm_rev_stub.cpp
  tests/stubs/sdl_stub.cpp
)
HARNESSES=(npbind playgo rif psf)

OUT="$SRC/mayhem/build"
mkdir -p "$OUT/fuzz" "$OUT/plain"

# compile <objdir> <flags...> : compile every SOURCES entry into <objdir> (parallel).
compile() {
  local dir=$1; shift
  printf '%s\n' "${SOURCES[@]}" | xargs -P "$MAYHEM_JOBS" -I{} sh -c '
    d=$0; o="$d/$(echo "{}" | tr / _).o"
    exec "$@" -c "{}" -o "$o"' "$dir" "$CXX" -std=c++23 "$@" "${DEFS[@]}" "${INC[@]}"
}

# 1) Sanitized + instrumented project objects for the fuzzers.
FUZZ_FLAGS=($SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link)
compile "$OUT/fuzz" "${FUZZ_FLAGS[@]}"
rm -f "$OUT/libproject-fuzz.a"; ar rcs "$OUT/libproject-fuzz.a" "$OUT"/fuzz/*.o

"$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$OUT/lsan_off.o"
"$CC" $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/standalone_main.o"

# 2) Each harness twice: libFuzzer binary + standalone reproducer.
for h in "${HARNESSES[@]}"; do
  "$CXX" -std=c++23 "${FUZZ_FLAGS[@]}" "${DEFS[@]}" "${INC[@]}" \
    -c "$SRC/mayhem/harnesses/fuzz_$h.cpp" -o "$OUT/fuzz_$h.o"
  "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OUT/fuzz_$h.o" "$OUT/lsan_off.o" \
    "$OUT/libproject-fuzz.a" "${LIBS[@]}" -o "$OUT/fuzz_$h"
  "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS "$OUT/standalone_main.o" "$OUT/fuzz_$h.o" "$OUT/lsan_off.o" \
    "$OUT/libproject-fuzz.a" "${LIBS[@]}" -o "$OUT/fuzz_$h-standalone"
done

# 3) Known-answer oracle with the project's normal flags (no sanitizers), run by test.sh.
compile "$OUT/plain" -O2 $COVERAGE_FLAGS
"$CXX" -std=c++23 -O2 $COVERAGE_FLAGS "${DEFS[@]}" "${INC[@]}" "$SRC/mayhem/selftest.cpp" \
  "$OUT"/plain/*.o "${LIBS[@]}" $COVERAGE_FLAGS -o "$OUT/selftest"

echo "built: $(ls "$OUT" | grep -E '^(fuzz_[a-z]+(-standalone)?|selftest)$' | tr '\n' ' ')"
