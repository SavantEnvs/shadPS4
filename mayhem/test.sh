#!/usr/bin/env bash
#
# mayhem/test.sh — run the known-answer parser tests that mayhem/build.sh built
# (mayhem/selftest.cpp against upstream's psf/npbind/playgo_chunk/rif code, normal flags).
# Upstream's own gtest suites need googletest from the network (FetchContent) and the full
# externals tree, so they are not built here.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

EXPECTED=20   # number of check() calls in mayhem/selftest.cpp
bin="$SRC/mayhem/build/selftest"
if [ ! -x "$bin" ]; then
  echo "test.sh: $bin missing — run mayhem/build.sh first" >&2
  emit_ctrf "shadps4-selftest" 0 "$EXPECTED"; exit 1
fi
scratch=$(mktemp -d)
out=$(TMPDIR="$scratch" "$bin" 2>&1); rc=$?
rm -rf "$scratch"
echo "$out"
passed=$(grep -c '^PASS ' <<<"$out" || true)
failed=$(grep -c '^FAIL ' <<<"$out" || true)
missing=$(( EXPECTED - passed - failed ))
[ "$missing" -gt 0 ] && failed=$(( failed + missing ))
[ "$rc" -ne 0 ] && [ "$failed" -eq 0 ] && failed=1
emit_ctrf "shadps4-selftest" "$passed" "$failed"
