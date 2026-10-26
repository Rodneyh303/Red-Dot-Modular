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
STD="-std=c++20"
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
  "test_slew_latch_regression|$PE"   # CC_PROMPT_slew_latch_regression: slew latches UNCONDITIONALLY (latchSlew) — ABMix lock must not gate it; probabilities change at the roll, macro + east
  "test_sweepsig_gate|$PE $SE $GS"   # CC_PROMPT_restore_sweepsig_gate_START: the RESTORED prob-out gate — fires on every fingerprinted input, skips unchanged, bit-identical skips, the own-slot defense (MacroOwn reads Macro's global cells only)
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
  "test_containment_edits|"   # Containment step 1: edit checks (self reject, ancestor-closure wavefront reject, valid accept, idempotent re-pin, remove/clear always valid) + raw uint16[16] serialize/deserialize round-trip identity (the WARPDIAG lesson) + reject-invalid-blob guard; seeded-Philox random-DAG march (600 seeds x 80 edits, validity asserted at every step)
  "test_containment_transforms|"   # Containment step 2: the DAG-closed gesture verbs — known-answers (spec chain invert, rotate/reflect conjugation, collapse union, collapse-all flat-single-root), per-verb closure property over seeded random DAGs (acyclic + no self-bits + save/loadable after EVERY verb), the plan's active-pool conjugation law via the sigma-pullback oracle, group laws (involutions, dihedral), composition marches (iterated collapse -> empty fixpoint; 200 DAGs x 100 mixed verbs), EXPAND lossiness (R3 seam)
  "test_containment_pending|"   # Containment step 3: the phrase-boundary pending machinery — latch/stamp rules (stamp on unarmed->armed only), hold-then-commit (latch never touches the graph), chronological order (COLLAPSE-ALL x INVERT non-commuting = order audible; invert x collapse naturality pinned), row-index tie-break, out-of-axis persistence (mask-0 defers, stamps kept, no jump-to-front), reset-when-empty, EXPAND trajectory (pre+post push works from the first commit, STOP at start, branching, edit-ride), ring serialise/deserialise (corrupt-state reject), rotate-step config, 400-seed batch closure march
  "test_containment_module|"   # Containment step 4: the MODULE-STATE core (ContainmentModuleCore, rack-free) — fresh-core defaults, the pin-edit path, THE CYCLE (edit->latch hold->boundary commit->undo publication + trajectory growth), the owner-guard protocol (driver A applies, driver B skips, window reopens), no-op batch publishes NO snapshot, the EXACT production save schema round-trips (object + TEXT), corrupt-save policies (malformed graph->R2 default, absent key->kept, stale/cyclic tail->dropped), SPSC undo ring (order continuity + drop-if-full), 60-seed march with mid-march save/load
  "test_distribution|"   # header-only: the Distribution probability core (parametrics + Pchip; jansson via test/jansson.h shim)
  "test_edge_cases|"
  "test_eradication_harness|$PE"
  "test_final_random_by_strand|"
  "test_fractional_tail|"
  "test_kumaraswamy_reparam|"   # header-only: Kumaraswamy (mode,conc) <-> (a,b) authoring map (conc:=a, closed-form b; CONC_MAX=5 load-bearing)
  "test_lane_direction|"
  "test_lane_bitcompare|$SE $GS $PE"
  "test_voice_count_partial|$PE"
  "test_simd_compat|"   # header-only: SIMD compat layer (SIMDe: native SSE on x86, NEON emulation on arm64)
  "test_lane_types|"
  "test_lane_voice_uniformity|$PE $SE $GS"   # SEAM_ERADICATION_PLAN Stage 0: the per-(voice,lane) uniformity harness. Known seams print but don't fail CI; UNIFORMITY_STRICT=1 = the per-stage gate.
  "test_legato_leading_edge|"
  "test_lock_behaviour|$PE"
  "test_macro_own_output|$PE $SE $GS"   # CC_PROMPT_macro_output_east_leak: the backwards-delegation contract — East's spread on an East-delegated lane must not reach Macro's prob OUT (who-responds at the source; the bars were already one-way-fixed)
  "test_mode_b_gate|$SE $GS $PE"
  "test_mode_b_gap|$SE $GS $PE"
  "test_gate_mode_agnostic|$SE $GS $PE"
  "test_gate_smoke|$SE $GS $PE $CE"
  "test_modes_bcd|"
  "test_modes_cd_microtonal|"
  "test_monsoon_discovery|"
  "test_module_lookup|"   # CC_PROMPT_MODULE_LOOKUP_AUTHORITY_START: the fourth authority — event-driven adjacency (onExpanderChange → topology epoch → memoized lookups); L5 is the raw-walk grep-gate — FATAL by default since the TAIL migration (template forms included; MODULE_LOOKUP_STRICT=0 = local debug hatch)
  "test_mono_dir_authority|"
  "test_monotone_curve|"   # header-only: the .dmcurve Phase 1 math module (jansson via test/jansson.h shim)
  "test_MpeMath|"
  "test_multistep|"
  "test_note_length|"
  "test_ownership_flip|$PE $SE $GS"   # layer-2 (ZAI_PROMPT_delegation_decision.md §4): the 16×7 owner-flip grid + warp-derivation pin — Rodney's 3-case rule fails a suite, not in ears
  "test_per_voice_articulation|$SE $GS $PE"
  "test_poly_slur_roll|"
  "test_poly_voices|"
  "test_probmod_roundtrip|"
  "test_qmix_poly|"
  "test_qmix_rng|"
  "test_quantize_engine|$SE $GS $PE"
  "test_rate_manager|"   # CC_PROMPT_RATE_MANAGER_START: the compute-rate authority — manager-dispatched producers + skip-on-unchanged fingerprints (Stage 0: lean core; the Macro sync population is the first producer)
  "test_resolver_dir_bridge|"   # header-only: resolver step-4 BYTE-IDENTICAL gate — BOTH direction pushes' moved selection algebra (DirPolicy via redDot::resolveDirItem) vs a verbatim copy of the OLD poly line (exhaustive 242-case grid + seeded 20k full-Inputs fuzz, int equality + same-BITS pre-round float). The direction pin; step 1's spread bridge is the template.
  "test_resolver_lor_bridge|"   # header-only: resolver step-3 BYTE-IDENTICAL gate — combineLOR's moved delegation algebra (LorPolicy via redDot::resolveLorItem) vs a verbatim copy of the OLD MonsoonExpanderManager combineLOR (seeded 120k-case fuzz, int equality + same-BITS pre-cast float). The LOR pin; step 1's spread bridge is the template.
  "test_resolver_spread_bridge|"   # header-only: resolver step-1 BYTE-IDENTICAL gate — SandsModifierResolver<SpreadPolicy> + the compat shell vs a verbatim copy of the OLD SpreadResolver formula (seeded 120k-case fuzz, same-BITS). The permanent pin; steps 2-4 extend the pattern.
  "test_resolver_warp_bridge|"   # header-only: resolver step-2 BYTE-IDENTICAL gate — WarpBank's resolver-routed mix/effective members vs a verbatim copy of the OLD WarpF.hpp formulas (seeded 120k-case fuzz, same-BITS mix + same resolved (f,w)). The warp pin; step 1's spread bridge is the template.
  "test_rule2_consume|"
  "test_rule2_integration|"
  "test_scale_list|"
  "test_scale_nondestructive|"
  "test_send_tap|"   # header-only: per-voice send-tap contract (SpreadResolver + MacroTapBank seams)
  "test_spread_resolver|"
  "test_ui_to_resolver|$PE $SE $GS"   # CC_PROMPT_UI_TO_RESOLVER_SPEC: the ONE-PRODUCER contract suite — effectiveProbability is the single producer (engine draw == bars == out, modulo the latch); ownership lenses, explicit voice, subsumption of the old formula; T4 (direction) + T5 (rate) structural halves land with their commits
  "test_subgate|$SE $GS $PE"
  "test_var_leg_rest|"
  "test_voice_channel_mapping|"   # header-only: pins poly output-channel(i+1)->V(i+2) and Lantern global-voice g->V(g+1) (the two off-by-ones: Rack showed row 2 missing / lane N drove row N+1 / V16 never output). RED on slot(i+1)/slot(pv_i+1).
  "test_voice_conventions|$SE $GS $PE"
  "test_voice_resolver|$SE $GS $PE"
  "test_warp_f_bank|"   # header-only: the per-voice warp-F variant bank (CC_PROMPT_02 — identity default, single-impl holders, branchless mix)
  "test_warp_stages|"   # header-only: FEEL-TEST MO rail-tilt + injected-atom stages (CC_PROMPT_TILT_AND_RAIL_FEELTEST) — neutral bit-exact, monotone, atom mass, tilt direction, edge continuity, persistence
  "test_warp_mirror|$PE $SE $GS"   # CC_PROMPT_03: the warp mirror + consumer routing (STEP 4 adds the SequencerEngine accessor)
  "test_warp_all_lanes|$PE $SE $GS"   # CC_PROMPT_05: ONE uniform source (warpRow) for every lane × voice — default no-op, warp-reaches, no-raw-leak
  "test_warp_voice_isolation|$PE $SE $GS"   # CC_PROMPT_07: per-voice warp isolation + the delegation-SOURCE contract (manager fills owner rows; no Macro→East leak; V1/V2 distinguished)
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
  pcfail=0; pcrun=0
  for s in $SHARED_SRCS; do
    o="$workdir/$(printf '%s' "$s" | tr '/.' '__').o"
    OBJ_OF["$s"]="$o"
    ( g++ $STD $INCS -c "$s" -o "$o" 2>"$o.err" || { echo "PRECOMPILE FAIL: $s"; cat "$o.err"; } ) &
    pcrun=$((pcrun+1))
    if [ "$pcrun" -ge "$jobs" ]; then wait; pcrun=0; fi
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

# dispatch in chunked batches of $jobs: launch up to $jobs, then plain `wait` for the
# WHOLE batch before the next. Uses only portable `wait` (no `wait -n`, which hangs on
# MSYS2/Git-Bash). Slightly less tightly packed than a rolling pool, but robust and
# guarantees every job finishes (and every .res exists) before collection.
idx=0; running=0
for spec in "${TESTS[@]}"; do
  name="${spec%%|*}"; srcs="${spec#*|}"
  [ -n "$filter" ] && [[ "$name" != *"$filter"* ]] && continue
  run_one "$idx" "$name" "$srcs" &
  idx=$((idx+1)); running=$((running+1))
  if [ "$running" -ge "$jobs" ]; then wait; running=0; fi
done
wait

# collect results IN ORDER (deterministic output, same as serial)
total=$idx
# map idx -> name (for reporting a result file that went missing)
nidx=0; declare -A NAME_OF
for spec in "${TESTS[@]}"; do
  nm="${spec%%|*}"
  [ -n "$filter" ] && [[ "$nm" != *"$filter"* ]] && continue
  NAME_OF[$nidx]="$nm"; nidx=$((nidx+1))
done
for ((i=0; i<total; i++)); do
  res="$workdir/$i.res"
  if [ ! -f "$res" ]; then
    printf "  \033[31m✗\033[0m  %-32s %s\n" "${NAME_OF[$i]:-?}" "NO RESULT (job did not complete)"
    fail=$((fail+1)); failed_names+=("${NAME_OF[$i]:-idx$i}"); continue
  fi
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
