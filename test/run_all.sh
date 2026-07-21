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

INCS="-Itest -Isrc -Isrc/dsp -Isrc/dsp/engines -Isrc/dsp/gates -Isrc/dsp/managers -Isrc/ui"
STD="-std=c++17"
GS="src/dsp/gates/GateState.cpp"
PE="src/dsp/engines/PatternEngine.cpp"
SE="src/dsp/engines/SequencerEngine.cpp"

# name|companion sources (empty = header-only). Keep alphabetical.
TESTS=(
  "test_GateState|$GS"
  "test_PatternEngine|$PE"
  "test_PhiloxRng|"
  "test_SandsTopology|"
  "test_SpreadInterp|"
  "test_SquaresRng|"
  "test_StepGate|$SE $GS $PE"
  "test_StoreEditAction|"
  "test_change_alley_remap|"
  "test_edge_cases|"
  "test_final_random_by_strand|"
  "test_fractional_tail|"
  "test_lane_direction|"
  "test_legato_leading_edge|"
  "test_lock_behaviour|$PE"
  "test_modes_bcd|"
  "test_mono_dir_authority|"
  "test_multistep|"
  "test_note_length|"
  "test_per_voice_articulation|$SE $GS $PE"
  "test_poly_slur_roll|"
  "test_poly_voices|"
  "test_probmod_roundtrip|"
  "test_rule2_consume|"
  "test_rule2_integration|"
  "test_scale_list|"
  "test_scale_nondestructive|"
  "test_spread_resolver|"
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
bin=/tmp/rdt_test_bin
for spec in "${TESTS[@]}"; do
  name="${spec%%|*}"; srcs="${spec#*|}"
  [ -n "$filter" ] && [[ "$name" != *"$filter"* ]] && continue
  src="test/$name.cpp"
  if [ ! -f "$src" ]; then
    printf "  \033[33m?\033[0m  %-32s (missing file)\n" "$name"; continue
  fi
  if ! g++ $STD $INCS "$src" $srcs -o "$bin" 2>/tmp/rdt_cerr; then
    printf "  \033[31m✗\033[0m  %-32s COMPILE ERROR\n" "$name"
    grep -iE "error:|undefined ref" /tmp/rdt_cerr | head -2 | sed 's/^/       /'
    fail=$((fail+1)); failed_names+=("$name"); continue
  fi
  out="$("$bin" 2>&1)"; code=$?
  tally="$(printf '%s' "$out" | grep -oiE "[0-9]+ (passed|pass)[a-z]*(,? *[0-9]+ fail[a-z]*)?" | tail -1)"
  if [ $code -eq 0 ]; then
    printf "  \033[32m✓\033[0m  %-32s %s\n" "$name" "${tally:-ok}"
    pass=$((pass+1))
  else
    printf "  \033[31m✗\033[0m  %-32s %s\n" "$name" "${tally:-RUNTIME FAIL (exit $code)}"
    fail=$((fail+1)); failed_names+=("$name")
  fi
  [ $verbose -eq 1 ] && printf '%s\n' "$out" | sed 's/^/       /'
done

echo "-----"
echo "suites: $((pass+fail))  passed: $pass  failed: $fail  (test_MeloDicer excluded: needs real SDK)"
if [ $fail -gt 0 ]; then
  echo "FAILED: ${failed_names[*]}"
  exit 1
fi
echo "all green"
