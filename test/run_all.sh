#!/usr/bin/env bash
# run_all.sh — compile and run every standalone unit test, one command, from repo root
# or from test/. This script is the SOURCE OF TRUTH for each test's compile line: the
# per-file "Compile:" header comments drifted (six different -I conventions, undocumented
# companion .cpp requirements), so trust this over them. Add new tests to TESTS below.
#
# Usage:
#   test/run_all.sh            # run all, summary + nonzero exit on any failure
#   test/run_all.sh -v         # also echo each test's own stdout
#   test/run_all.sh gsstep     # run only tests whose name matches the filter
#
# Why companion sources: a few engine units are split .hpp/.cpp (GateState, PatternEngine,
# SequencerEngine), so their tests must LINK the matching .cpp. Header-only tests list none.
# test_MeloDicer is intentionally excluded — it needs the real Rack SDK header, not the
# test/ shim, so it only builds in the plugin tree.
set -u

# resolve repo root whether invoked from root or test/
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"
cd "$root"

# python for the anchor/bind audit: python3 (Linux/macOS) or python (MSYS2/Windows)
PYBIN="$(command -v python3 || command -v python || true)"

INCS="-Itest -Isrc -Isrc/tuning -Isrc/dsp -Isrc/dsp/engines -Isrc/dsp/gates -Isrc/dsp/managers -Isrc/ui"
# SIMDe (for the SIMD Philox compat test) lives in the Rack SDK's dep/include.
# Add it when RACK_DIR is set (CI sets it); tests not needing SIMDe are unaffected.
if [ -n "${RACK_DIR:-}" ] && [ -d "${RACK_DIR}/dep/include" ]; then
  INCS="$INCS -I${RACK_DIR}/dep/include"
elif [ -d "../../dep/include" ]; then
  INCS="$INCS -I../../dep/include"
fi
STD="-std=c++17"
# SSE4.1 for SIMD Philox (x86 only; arm64 uses SIMDe NEON emulation, no flag needed)
ARCHF=""
if [ "$(uname -m)" = "x86_64" ] || [ "$(uname -m)" = "i686" ] || [ "$(uname -m)" = "i386" ]; then
    ARCHF="-msse4.1 -mavx"   # SSE4.1 for Philox int ops, AVX1 for sumZ double _mm256 dot product
fi
STD="$STD $ARCHF"
# Match the plugin's deterministic-FP flags (Makefile DETERMINISTIC_FP) so bit-exact float tests
# (EXPECT_BITEQ in test_TuningTable etc.) hold across x86 and arm64. Without -ffp-contract=off, arm64
# (Mac) contracts (a*b)*c into an FMA that rounds differently from x86 -> last-bit diff -> false failure.
STD="$STD -ffp-contract=off"   # DECIDED: match Makefile DETERMINISTIC_FP — dropped reassoc guard (nil value), kept -ffp-contract=off (cross-platform FMA determinism; keeps bit-exact tests valid across x86/arm64)
GS="src/dsp/gates/GateState.cpp"
PE="src/dsp/engines/PatternEngine.cpp"
SE="src/dsp/engines/SequencerEngine.cpp"
CE="src/dsp/engines/ClockEngine.cpp"

# name|companion sources (empty = header-only). Keep alphabetical.
TESTS=(
  "test_12tet_identity|$SE $GS $PE"
  "test_GateState|$GS"
  "test_GaussianCopula|"
  "test_PatternEngine|$PE"
  "test_PhiloxRng|"
  "test_SandsTopology|"
  "test_SpreadInterp|"
  "test_slew_copula|src/dsp/engines/PatternEngine.cpp"
  "test_SquaresRng|"
  "test_ScalaFile|"
  "test_ScaleMaskArbiter|"
  "test_StepGate|$SE $GS $PE"
  "test_TuningList|"
  "test_TuningRoundTrip|"
  "test_TuningTable|"
  "test_StoreEditAction|"
  "test_ca_qmix_source_select|"
  "test_ca_true_reverse|"
  "test_ca_pending_order|"
  "test_change_alley_transforms|"
  "test_edge_cases|"
  "test_final_random_by_strand|"
  "test_fractional_tail|"
  "test_lane_direction|"
  "test_lane_bitcompare|$SE $GS $PE"
  "test_voice_count_partial|$PE"
  "test_simd_compat|"   # header-only: SIMD compat layer (SIMDe: native SSE on x86, NEON emulation on arm64)
  "test_lane_types|"
  "test_legato_leading_edge|"
  "test_lock_behaviour|$PE"
  "test_mode_b_gate|$SE $GS $PE"
  "test_mode_b_gap|$SE $GS $PE"
  "test_gate_mode_agnostic|$SE $GS $PE"
  "test_gate_smoke|$SE $GS $PE $CE"
  "test_modes_bcd|"
  "test_modes_cd_microtonal|"
  "test_monsoon_discovery|"
  "test_mono_dir_authority|"
  "test_MpeMath|"
  "test_multistep|"
  "test_note_length|"
  "test_per_voice_articulation|$SE $GS $PE"
  "test_poly_slur_roll|"
  "test_poly_voices|"
  "test_probmod_roundtrip|"
  "test_qmix_poly|"
  "test_qmix_rng|"
  "test_quantize_engine|$SE $GS $PE"
  "test_rule2_consume|"
  "test_rule2_integration|"
  "test_scale_list|"
  "test_scale_nondestructive|"
  "test_spread_resolver|"
  "test_subgate|$SE $GS $PE"
  "test_var_leg_rest|"
  "test_voice_resolver|$SE $GS $PE"
)

verbose=0; filter=""
for a in "$@"; do
  case "$a" in
    -v) verbose=1 ;;
    *)  filter="$a" ;;
  esac
done

pass=0; fail=0; failed_names=()

# ── Parallel execution ────────────────────────────────────────────────────────
# Each test (compile + run) is independent, so fan them out across cores. Set
# RDT_JOBS to override; default = CPU count. RDT_JOBS=1 forces the old serial order.
jobs="${RDT_JOBS:-$( (command -v nproc >/dev/null && nproc) || sysctl -n hw.ncpu 2>/dev/null || echo 4 )}"
workdir="$(mktemp -d /tmp/rdt_run.XXXXXX)"
trap 'rm -rf "$workdir"' EXIT

# ── Precompile shared sources once ────────────────────────────────────────────
# GateState/PatternEngine/SequencerEngine/ClockEngine are each linked by many tests
# (PE by 15, GS by 12, SE by 11). Compiling them per-test dominates runtime. Build
# each to a .o ONCE here (in parallel), with the IDENTICAL flags tests use, then the
# per-test compile links the .o instead of recompiling the .cpp. Set RDT_NO_PRECOMPILE=1
# to disable (falls back to compiling sources directly, as before).
SHARED_SRCS="$GS $PE $SE $CE"
declare -A OBJ_OF
if [ "${RDT_NO_PRECOMPILE:-0}" != "1" ]; then
  pcfail=0
  for s in $SHARED_SRCS; do
    o="$workdir/$(printf '%s' "$s" | tr '/.' '__').o"
    OBJ_OF["$s"]="$o"
    ( g++ $STD $INCS -c "$s" -o "$o" 2>"$o.err" || { echo "PRECOMPILE FAIL: $s"; cat "$o.err"; exit 1; } ) &
    while [ "$(jobs -r | wc -l)" -ge "$jobs" ]; do wait -n 2>/dev/null || wait; done
  done
  wait
  # if any .o is missing (compile failed), abort precompile mode cleanly -> fall back to direct
  for s in $SHARED_SRCS; do [ -s "${OBJ_OF[$s]}" ] || pcfail=1; done
  if [ "$pcfail" = "1" ]; then
    echo "  (precompile failed; falling back to per-test source compile)"
    OBJ_OF=()
  fi
fi

# translate a test's source list into .o's where precompiled, else leave the .cpp
objs_for() {
  local out="" tok
  for tok in $1; do
    if [ -n "${OBJ_OF[$tok]:-}" ]; then out="$out ${OBJ_OF[$tok]}"; else out="$out $tok"; fi
  done
  printf '%s' "$out"
}

# Run one test: compile to a unique bin, run it, write a self-contained result record
# to $workdir/<idx>.res. Record format (one file per test):
#   line 1: status  (OK | FAIL | COMPILE | MISSING)
#   line 2: name
#   line 3: tally/message (what prints after the name)
#   rest:   captured stdout (for -v) OR compile errors (for COMPILE)
run_one() {
  local idx="$1" name="$2" srcs="$3"
  local res="$workdir/$idx.res"
  local src="test/$name.cpp"
  local bin="$workdir/bin_$idx"
  if [ ! -f "$src" ]; then
    printf 'MISSING\n%s\n(missing file)\n' "$name" > "$res"; return
  fi
  local link; link="$(objs_for "$srcs")"
  if ! g++ $STD $INCS "$src" $link -o "$bin" 2>"$workdir/cerr_$idx"; then
    { printf 'COMPILE\n%s\nCOMPILE ERROR\n' "$name"
      grep -iE "error:|undefined ref" "$workdir/cerr_$idx" | head -2; } > "$res"; return
  fi
  local out code tally
  out="$("$bin" 2>&1)"; code=$?
  tally="$(printf '%s' "$out" | grep -oiE "[0-9]+ (passed|pass)[a-z]*(,? *[0-9]+ fail[a-z]*)?" | tail -1)"
  if [ $code -eq 0 ]; then
    { printf 'OK\n%s\n%s\n' "$name" "${tally:-ok}"; printf '%s\n' "$out"; } > "$res"
  else
    { printf 'FAIL\n%s\n%s\n' "$name" "${tally:-RUNTIME FAIL (exit $code)}"; printf '%s\n' "$out"; } > "$res"
  fi
}

# dispatch, capped at $jobs concurrent
idx=0
for spec in "${TESTS[@]}"; do
  name="${spec%%|*}"; srcs="${spec#*|}"
  [ -n "$filter" ] && [[ "$name" != *"$filter"* ]] && continue
  run_one "$idx" "$name" "$srcs" &
  idx=$((idx+1))
  # throttle: wait if we've hit the job cap
  while [ "$(jobs -r | wc -l)" -ge "$jobs" ]; do wait -n 2>/dev/null || wait; done
done
wait

# collect results IN ORDER (deterministic output, same as serial)
total=$idx
for ((i=0; i<total; i++)); do
  res="$workdir/$i.res"
  [ -f "$res" ] || continue
  status="$(sed -n '1p' "$res")"; rname="$(sed -n '2p' "$res")"; msg="$(sed -n '3p' "$res")"
  case "$status" in
    MISSING) printf "  \033[33m?\033[0m  %-32s %s\n" "$rname" "$msg" ;;
    COMPILE) printf "  \033[31m✗\033[0m  %-32s %s\n" "$rname" "$msg"
             sed -n '4,5p' "$res" | sed 's/^/       /'
             fail=$((fail+1)); failed_names+=("$rname") ;;
    OK)      printf "  \033[32m✓\033[0m  %-32s %s\n" "$rname" "$msg"
             pass=$((pass+1))
             [ $verbose -eq 1 ] && sed -n '4,$p' "$res" | sed 's/^/       /' ;;
    FAIL)    printf "  \033[31m✗\033[0m  %-32s %s\n" "$rname" "$msg"
             fail=$((fail+1)); failed_names+=("$rname")
             [ $verbose -eq 1 ] && sed -n '4,$p' "$res" | sed 's/^/       /' ;;
  esac
done

echo "-----"
echo "suites: $((pass+fail))  passed: $pass  failed: $fail  (test_MeloDicer excluded: needs real SDK)  [jobs: $jobs]"

# ── anchor-vs-bind audit (SvgPanelKit geometry guard) ─────────────────────────
# Every generated panel anchor must have a widget bind and vice-versa; catches the
# "control is missing / panel and widget drifted" bug class as a suite failure.
# Skipped by the name filter unless the filter matches "audit".
audit_fail=0
if [ -z "$filter" ] || [[ "audit" == *"$filter"* ]]; then
  echo "anchor/bind audit:"
  if [ -z "$PYBIN" ]; then
    printf "  \033[33m?\033[0m  anchor/bind audit SKIPPED (no python found)\n"
  elif ! "$PYBIN" "$here/audit_anchor_bind.py"; then
    audit_fail=1
  fi
  echo "-----"
fi

if [ $fail -gt 0 ] || [ $audit_fail -ne 0 ]; then
  [ $fail -gt 0 ] && echo "FAILED: ${failed_names[*]}"
  [ $audit_fail -ne 0 ] && echo "FAILED: anchor/bind audit"
  exit 1
fi
echo "all green"
