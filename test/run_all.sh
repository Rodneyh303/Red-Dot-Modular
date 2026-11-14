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
  "test_pattern_length_cap|$SE $GS $PE $CE"   # STEPS_32: the length-domain pins — the constants (cap == 32 since the 2b flip), the master-window wrap sweep 1..cap (visited set == [0,L), mask popcount L, period exactly L), the wrap PERIOD at offsets (the Stage-1 cases + the new 32-bound cases), the clamp report (over-range requests clamp to the cap; 17..32 deliver in range now), the CV map (D1's *31: 10V -> 32, 5V -> 17), the lane clamp via setStrand + the lane own-length wrap sweep
  "test_slew_copula|src/dsp/engines/PatternEngine.cpp"
  "test_slew_latch_regression|$PE"   # CC_PROMPT_slew_latch_regression: slew latches UNCONDITIONALLY (latchSlew) — ABMix lock must not gate it; probabilities change at the roll, macro + east
  "test_sweepsig_gate|$PE $SE $GS"   # CC_PROMPT_restore_sweepsig_gate_START: the RESTORED prob-out gate — fires on every fingerprinted input, skips unchanged, bit-identical skips, the own-slot defense (MacroOwn reads Macro's global cells only)
  "test_SquaresRng|"
  "test_stage2b_flip32|$SE $GS $PE $CE"   # STEPS_32 Stage 2b's own pins (the plan, run after the two REQUIRED PRE-GATES): the factory default window rides the RING (wrap at 32, both bars reachable, the bar-boundary walk 15->16->31->0; cachedLength still reports the knob default 16 and the module-default push resolves (16,0) — the live <=16 world), mask bits address ALL 32 steps (painted single cells 0..31 through the real pushMaskEditor + a local east bit at cell 20 + the all-on sentinel + the >=32 drop, the 64-headroom law), and the draw-window headroom math pinned (static_asserts: DRAW_CHUNK == ring*64 == the rhythm fill, the buffers hold the fills, the unit()-cursor worst cases fit; runtime: the 2048/1024/512 fill relationships)
  "test_stage2b_pregates|$SE $GS $PE $CE"   # STEPS_32 Stage 2b REQUIRED PRE-GATES (plan: "REQUIRED PRE-GATES (from the 2a review)") — GATE 1: the non-default <=16 equivalence pin (the mis-read detector): varied per-step probabilities on every lane x voice row (a cross-row read reads the NEIGHBOUR's value), painted macro+east masks + an offset/length window + a MASK_DYNAMIC voice, non-default ratchet/ghost REST/LEG/ADV/ACC thresholds, 6 marches x 64 steps x the REAL executors, every observable (decision/nv/gate/CV all 16 voices, walk display state, mask projections, melody + published-slew row mirrors) digested WHOLE + PER STEP vs the committed oracle test/goldens/steps32_gate1_cap16_ref.txt (EQUIV lines compare at every cap; CAP16WRAP lines — the D3-documented wrap configs — compare only while cap==16; record: <bin> --record once at the 2a tip) — GATE 2: the beyond-ring zero-fill pin: every per-step row's cells >= kPatternLengthCap exactly zero (random_/warped_/slewed*/preRemapSlewed*/remapSnap*/source caches/melody mirrors/draw scratch/cached-draw z rows/z0-z1 window caches) + the uint64 mask planes' bits beyond the ring clear, sampled at t=0, every 8 steps, and end-of-run (a leak can be transient); caught its first real leak on the maiden run: PatternEngine::reset() seeded its factory defaults storage-wide (1536 nonzero cells past the ring)
  "test_stage4_persistence|$SE $GS $PE $CE"   # STEPS_32 Stage 4 — the control + persistence gate (plan: "Pattern-length parameter, persisted; absent key → 16. Gate: old patches load at 16 unchanged; round-trip 32"): S0 the VALUE layer (the knob domain 1..32 + mid-drag fractions round-trip the json-double encoding exactly, the round() landing is a pure function of the stored value) — S1 the SOURCE facts (the test_module_lookup L5 grep-gate precedent): exactly ONE configParam site per param in MonsoonConfigurator and its spelling is the cap-derived default-16/0 one (D2), NO source line anywhere writes PATTERN_LENGTH_PARAM / PATTERN_OFFSET_PARAM (the knob answers only to the user and Rack's patch restore — a load hook that snaps the knob is the ruled-out failure class), and NO second persisted home (no "patternLength" json key — one value, one home, the MODE LAW) — S2 ABSENT KEY → 16 (the default push lands (16,0), the walk stays in [0,16), the default push marches BIT-IDENTICALLY to the authored (16,0) window) — S3 ROUND-TRIP 32 (lengths 1..32 x offsets {0,7,31} restore EXACTLY through the updateWindow call — no residual clamp, mask == windowMaskBits; the restored 32-window wraps at 32 with both bars reachable; the restored (32,0) and (32,7) pushes march BIT-IDENTICALLY to their authored twins — the Philox draw stream cannot shift on reload) — S4 OLD PATCHES ≤16 UNCHANGED (lengths 1..16 x offsets {0,5,15} restore exactly; the old-patch wrapping configs (12,10) the bar-crossing D3 extension and (16,5) the full old ring shifted march BIT-IDENTICALLY restored vs authored — GATE 1 owns the cross-flip equivalence, this owns the restore path)
  "test_stage5_combined|$SE $GS $PE $CE"   # STEPS_32 Stage 5 — verify (the plan's combined case, "per the earlier lesson that clean merges can still have interaction bugs"): the FULL-LOADOUT marches at length 32 through the REAL executors (A/B/BS, 96 steps = three ring passes each) — varied probability rows over ALL 32 cells x per-voice subgate thresholds x painted macro+east patterns with bits in BOTH bars x a MASK_DYNAMIC voice with a bar-2 region (10/22, its LOR's LOCAL-EAST flag set) x lanes whose own walks cross the bar line (rhythm 20/0, melody 32/0, octave 7/27 wrapping) x bar-2 poly windows x 15 voices + the legacy star graph: V1 determinism (each march built+run twice — whole + per-step content digests + all 16 per-stream digests bit-identical) + GATE 2's post-flip bite (the uint64 mask planes' bits >= ring clear at t=0/every 8/end) — V2 wrap at 32 + BOTH BARS through the executors (the mode-A walk covers ALL 32 cells with period EXACTLY 32 — the seed is phase-dependent, the laws are bound/period/distinctness/crossings — globalWalk_ tracks the landed cell, the 15->16 crossing and the 31->0 ring wrap in sequence, non-rest decisions in both bars, a melody DRAW fills the ring, the lane walks cross/wrap) + the DETERMINISTIC subgate/advance crossing (subResLive ADV=1: a RATCHET edge walks the shared playhead 14->15->16 across the bar line, a GHOST edge wraps 31->0 in the gap and plays) — V3 the display law under the combined loadout (BAR LEDs exclusive + follow stepIndex's bar layer, the lamp-0 tell, the unique playhead lamp, the standing levels only with the TRAIL's 0.5 reading the trail master's own membership, NO ghosts at the full window, the trail on lamp 15 at both mode-A ring wraps, the CURRENT-LAP ghost set at the (16,24) D3 wrap window) — V4 the ONE mask gate LIVE on the marches (outputMaskPasses == the authored painted/dynamic model at EVERY landed frame incl. bar 2, the effectiveOutputMask* projections, a painted-off bar-2 cell (16) refuses the mono output even with hot subgate thresholds — the mute multiplies post-engine) — V5 the bar-1 EXTENSION equivalence (the (32,0) march's first bar BIT-IDENTICAL to the (16,0) march's whole walk — content + mono + all 15 voice substreams, frames 0..15, the walk coordinate captured at its <=16 PROJECTION (setWindow re-anchors it mod newLen — the GATE 1 low16 discipline), the WALKING loadout: mono main-advance 0.95 for both twins — then they diverge: D3's extension, not alteration) — V6 the bar-2 ISOLATION twin (rotating voice 6's bar-2 poly rows moves only voice 6's stream; the mono lane and the other 14 leaf voices bit-identical ALL march — the cross-row mis-read detector at 32) — V7 the combined loadout's <=16 face ((16,0): landings stay at -1 (the pre-start park before the first edge) or in [0,16), BAR 1 lit throughout, no ghosts, the bar-2 mirror cells stay zero before any draw)
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
  "test_cantonment_edits|"   # Cantonment step 1: edit checks (self reject, ancestor-closure wavefront reject, valid accept, idempotent re-pin, remove/clear always valid) + raw uint16[16] serialize/deserialize round-trip identity (the WARPDIAG lesson) + reject-invalid-blob guard; seeded-Philox random-DAG march (600 seeds x 80 edits, validity asserted at every step)
  "test_cantonment_transforms|"   # Cantonment step 2: the DAG-closed gesture verbs — known-answers (spec chain invert, rotate/reflect conjugation, collapse union, collapse-all flat-single-root), per-verb closure property over seeded random DAGs (acyclic + no self-bits + save/loadable after EVERY verb), the plan's active-pool conjugation law via the sigma-pullback oracle, group laws (involutions, dihedral), composition marches (iterated collapse -> empty fixpoint; 200 DAGs x 100 mixed verbs), EXPAND lossiness (R3 seam), the anchored flatten + the reduction (2026-10-27), the RANDOM draws (row 7: seedForest the outright stateless page + scatterEdges the state-dependent pass — validity/determinism/lone-root/V1-free/the reverse-as-undo + the CA scrub pins + the scatter POOL SPAN: held rows, pool-masked remove, the pool-1 no-op, default==explicit pool)
  "test_cantonment_pending|"   # Cantonment step 3: the phrase-boundary pending machinery — latch/stamp rules (stamp on unarmed->armed only), hold-then-commit (latch never touches the graph), chronological order (COLLAPSE-ALL x INVERT non-commuting = order audible; invert x collapse naturality pinned), row-index tie-break, out-of-axis persistence (mask-0 defers, stamps kept, no jump-to-front), reset-when-empty, EXPAND trajectory (pre+post push works from the first commit, STOP at start, branching, edit-ride), ring serialise/deserialise (corrupt-state reject), rotate-step config, 400-seed batch closure march, the RANDOM row (page counter fwd/rev, the outright reverse-as-undo EXACT restore, the latest-arm direction rule, the scatter scrub oracle, the scatter POOL SPAN at polyphony 10: the pool oracle + held rows + the surviving above-pool pin), the DECLUTTER row (the reduction gesture: known answer, closure-preserved, idempotent, counter untouched, trajectory stepBack, stamp-order compose with collapse-all), the TRUE-REV queue (the global-reverse consumer: not a verb row, no stamp, applyAll does not consume it — the core does, before the verb bracket)
  "test_cantonment_module|"   # Cantonment step 4: the MODULE-STATE core (CantonmentModuleCore, rack-free) — fresh-core defaults, the pin-edit path, THE CYCLE (edit->latch hold->boundary commit->undo publication + trajectory growth), the owner-guard protocol (driver A applies, driver B skips, window reopens), no-op batch publishes NO snapshot, the EXACT production save schema round-trips (object + TEXT, incl. the RANDOM row's signed page counter + mode + density), corrupt-save policies (malformed graph->R2 default, absent key->kept, stale/cyclic tail->dropped, RANDOM keys absent->defaults/out-of-range->ignored), SPSC undo ring (order continuity + drop-if-full), the RANDOM commit publishes undo + trajectory (requirement 3/4 inherited), 60-seed march with mid-march save/load, the TRUE-REV consume (the global-reverse consumer, the CA engine mirrored: pure consume restores the previous committed state with NO undo — the modulation-class rule; mixed boundary brackets only the verbs; uncommitted pin drift discarded; out-of-axis stays queued; buffer-start STOP; branching; the RANDOM commit replays back)
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
  "test_lantern_bars|$SE $GS $PE $CE"   # STEPS_32 Stage 3 — the DECIDED Lantern gate (16 cols always, follows the disc, BOTH BARS RECORDED): the bar-aware write path (bar = step / 16, column = step % 16) pinned against the REAL executor marches — a 32-step run records steps 0-15 into the bar-1 buffer and 16-31 into bar-2 (the old bar-blind step >= 16 guard DROPPED them), the displayed bar equals the engine bar index (stepIndex / 16) on every step incl. the boundary landing, the content routing carries each step's live pitch into its own slot (per-cell varied melody — a mis-routed write cannot alias), the lap formula is ABSOLUTE (arrival only at the ring wrap 31->0 / reverse 0->31 and against-travel jumps, NEVER the mid-phrase 15->16 bar crossing), the (20,0) live window plays through the bar line (bar-2 records exactly the window's 4 bar-2 cells; the window wrap 19->0 IS a lap), and <=16 is unchanged (the (16,0) window keeps every write in bar-1 = the full history, bar-2 byte-default)
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
  "test_note_length|$PE"   # DOTTED BUILD: the varyNoteIndex rank-window pins need PatternEngine.cpp
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
  "test_rhythm_decision_core|"   # STAGE R R-3: the five named predicates (C1-C5) table-driven over every MonoDecision x own-state combination — the pure-layer spec (grows with the ladder: R-4 tuple assembly, R-5 star data)
  "test_rhythm_decision_differential|$SE $GS $PE $CE"   # STAGE R R-1: the reference oracle — 24 seeded marches x 64 steps x 3 modes (clock / plain gate / gate+subgates) through the REAL executors + the shipped IMPL 2b drive; per-voice gate/accent/pitch/decision streams -> per-mode FNV-1a golden digests (the byte-identity pin for every Stage-R commit); INV-C2 (mono rest closes all poly gates), INV-ENV (poly gate never exceeds the mono envelope), INV-DET (double-run determinism)
  "test_forest_rhythm|$SE $GS $PE $CE"   # STAGE F F-1: the FOREST behavioral spec — free voices (E(v)=Own(v): mono rests do NOT close them, gates rise while mono is low, own-window self-clocking), poly containers (V3 in V2 follows V2, not V1; the captured per-container view), the star contrast (the legacy invariants hold on the AUTHORED graph), downward-edge determinism (the documented F-2 caveat)
  "test_ring_bar_indicator|$SE $GS $PE $CE"   # STEPS_32 Stage 3 — the ring + the inner-disc bar indicator (P6's ring-map pins, D10 as re-scoped by D15): the 16-STAY twice-around form (lamp k carries masters k and k+16; the rendered master is the CURRENT LAP's — D10's split test on the playhead) pinned lamp-by-lamp per landed frame against the D15 model oracle (playhead 1.0 > trail 0.5 > in-window note 0.35 > ghost strictly below the 0.07 rest — the hierarchy is the law); the ≤16 face bit-identical and STABLE ((16,0): no lap flip, no ghost renderable, BAR 1 lit throughout); the full window renders NO ghosts and the BAR LEDs alone carry the lap (exclusive, follow stepIndex, 4 flips over two passes + the wrap home, invariant 2 — the new bar's LED lit on the lamp-0 tick); the ghost arc at a D3 wrap window == the flip-table model's CURRENT-LAP set (scripts/d15_ring_flip_table.py); the straddling window flips ONCE per pass at the tick crossing and the window wrap restores lap 0; the TRAIL REPAIR (the 2b flip killed the comet trail on the whole second bar — trailIdx ≥ 16 could never match a 0..15 lightIdx; the lamp landing restores it incl. across the bar line); mode D and the pre-start -1 park leave both BAR LEDs silent (the % 16 landing guarded); the atomic re-scope (indicator + ghost set + playhead lamp move within the ONE landed frame)
  "test_scale_list|"
  "test_scale_nondestructive|"
  "test_send_tap|"   # header-only: per-voice send-tap contract (SpreadResolver + MacroTapBank seams)
  "test_sands_grid_windows|$SE $GS $PE $CE"   # STEPS_32 Stage 3 — the Sands probability grid's DIMMING RULE gate (the plan's "## DIMMING RULE" block): the grid's in-window cell set (the VERBATIM-extracted SandsLaneWindowCore — the rack-free testable surface; the widget's ProbabilityLane inherits it and the widget-side static_assert pins STEP_COUNT == kPatternLengthCap, compiled by the TU smoke) equals SequencerEngine::windowMaskBits's engine set — R1: EVERY off+len<=16 window phrase-wide (the <=16 dimming unchanged); R2/R3: per BAR-1/BAR-2 half for the named classes (the module default, bar-2-only, the crossing, the WRAP past 32, the straddle, the full phrase, single cells at the head/bar-2/ring-end, a short wrap, the D3 (20,25) class); R4: both bars light their intersections; R5: display only (setDisplayLOR moves the visible window without touching the edit values, the divergence flag fires, sync restores, the set is rotation-invariant); the edit mirror + windowIndexOf across the wrap; the clamp defence yields engine-agreeing windows even over-range
  "test_range_strip_gesture|"   # CC_FIX_VOICE_RANGE_STRIP_INTERACTION — the voice-range strip's THREE interaction faults (the rack-free SandsRangeGestureCore surface; the widget consumes it + the TU smoke pins the wiring): A the edge drags are delta-relative from the PRESS window (delta 0 == the press window — NO engagement jump; one cell at a time; the other edge stays; exhaustive on the ring), B the edge zones are >= max(1 cell, 8px) EACH side of the edge boundary (the body never captures within one cell of an edge; beyond is the paint arm's NONE; tiny + wrapped windows), C the rotation drag is the LANES' rule (window-relative, clamped [0, len-1] — the chevron (off+rot) is ALWAYS in-window, exhaustive (off,len,step)); + the WINDOW slide law pinned verbatim (behave as before); D the preset bank gone (grep-in-test), E the lane edge drags run the strip's press-anchored law (one core, both owners); F/G (§F+G) the lane-window constants at the cap (kLaneOffsetMax 31 / kLaneLengthMax 32, static_assert + runtime, the gesture ring == the length max) and NO bounding literal in the two managers' LIVE lane-window code (comments stripped — the 0..15/1..16 float pairs and the % 16 wraps, the midpoint-snap class)
  "test_LorDrag|$SE $GS $PE $CE"   # LOR_EDITING_BUG_LIST §PLAN 1+3 — THE DRAG REDUCER (lorDragReduce: the press/move/release state machine extracted from the GLFW handlers — the doc's ROOT ISSUE; the widget is now the translation layer) + its widget-model gate suite: 1 the edge drags one-cell-at-a-time with NO jump at engagement (START+END, a lane and the strip), 2 past 16 and past 31 — no snap at the old boundary, no wrap anomaly at the seam, offsets 16..31 APPLY (the F-domain at the reducer level), 3 ONE OWNER PER DRAG (a lane press never writes the voice window nor fires onRangeEdit; a strip press never writes a lane; the setRangeState push guard holds; the commits fire on the right surface with the before/after pair), 4 the press's zone rides the drag (NONE never engages; moves on an inactive drag are no-ops), 5 rotation in [0, len-1] for EVERY (len, off, step) and re-clamped into a resized window, 6 the body drag slides with the grab offset held (len/rot preserved, wrapping both directions); 7 (bug 9) the wrapped window's body is grabbable (drag past 32, release, press in the body, move); 8 (bug 10) a length-1 window's bar moves it, the outside bands resize (Rodney's ruling); 9 (bug 11) the strip's tint draws from the ONE windowContains law (the exhaustive oracle + the delegation pins); 10 (bug 12) the chevron's x-extent is exactly one step width; 11 (bug 13) the RAW cell index — a drag toward the grid edge crosses the seam continuously (the wrapped one-step-side window drags back); 12 (bug 14) the 1-wide outside handle bands clamp into the grid rect; 13 (bug 15) the strip's window figure meets the minimum contrast ratio (the constants pinned, the old grey-on-grey recorded); 14 (bugs 17+18) the SHARED lane playhead (lanePlayheadBar) is in-window for EVERY (off,len,rot,tick) — the VAR scan exception retired everywhere (the delegation + no-caller pins); 15 (bug 17) the identity voice's read cell folds on the RING — a bar-2 global window keeps the strip's playhead in bar 2 (the old %16 fold pinned dead)
  "test_sands_snap_geom|"   # header-only: the Sands snap magnet's PURE near-miss decision (ui/SandsSnapGeom.hpp) — same-row gap window [0,2] grid units, mirroring Rack v2 updateExpanders' exact rounded-grid-box equality (the "subpanels not snapping together" fix)
  "test_spread_resolver|"
  "test_ui_to_resolver|$PE $SE $GS"   # CC_PROMPT_UI_TO_RESOLVER_SPEC: the ONE-PRODUCER contract suite — effectiveProbability is the single producer (engine draw == bars == out, modulo the latch); ownership lenses, explicit voice, subsumption of the old formula; T4 (direction) + T5 (rate) structural halves land with their commits
  "test_unified_addressing|$SE $GS $PE $CE"   # CC_PROMPT_UNIFIED_ADDRESSING_SPEC Stage 1: the nested global->voice->lane chain, GLOBAL map first — the PASS-THROUGH GOLDEN PIN (six FNV-1a march digests captured pre-change: default config == GOLDEN_A/B/BS, and NON-DIVISIVE lane lengths 7/5/6 with pendulum/pingpong walks bit-identical — the case a bounded driver would break), the MAPPED RULES (the KNOWN-ISSUE fix: an authored window makes the driver the window walk, so default lanes read INSIDE the global window and lanes with their own windows compose against the container length), the driverTickAt unit table, reversibility (backward retraces forward), re-authoring re-bounds the reads
  "test_output_mask_gate|$SE $GS $PE $CE"   # LOR_EDITING_BUG_LIST bug 16 (the AUDIO regression report) answered with the doc's demanded pins, END-TO-END from the editor-store shapes through pushMaskEditor to the composed gate at every global step: 1 the all-cycle default == NO gate (every voice every step), 2 cycle + painted bits off — exactly those steps silenced, the Macro-tier voices unaffected, 3 painted CHANGED -> the gate changes (the written-but-never-read smell becomes a test), 4 abs1 + a non-full LOR — silent outside [O,O+L) incl. the WRAPPED window and the full-window identity, 5 the Macro tier's paint gates the followers, 6 the OUTPUT-STAGE structural pin (MonsoonOutputGenerator applies outputMaskPasses at engine.stepIndex on BOTH the mono and poly paths — the composition is LIVE), 7 the write-only maskPainted_ echo is GONE (the working read is the anchored copy). The suite RECORDS the audit: the report's located cause is disproven on this branch — the working read path is the anchored copy via effectiveOverlayPattern, the composition is outputMaskPasses, and the output stage calls it; the reads stay ungated BY DESIGN (b134490's model)
  "test_16_triage|$SE $GS $PE $CE"   # CC_PROMPT_16_TRIAGE (option C, Rodney's ruling) + Prompt 2 (the cycle-mode phrase anchor): "16" means THREE things — each occurrence TRIAGED, never blanket-replaced. GATE 1 THE GREP PIN: no anonymous 16/15 in the engine's CODE (comments stripped, word-boundary tokens) — every one is kPatternLengthCap, kVoiceSlots, kDefaultPatternLen, kRingLamps, or the allowlisted bucket-1 polymeter arms (len == 16 && off == 0 VERBATIM at driverTickAt/driverTickBefore + the passG twins) — the DURABLE no-untriaged-16 law; GATE 2 the pass-through arms (the 16 arm verbatim + the CAP arm: the full-ring window passes through, driverTickAt unit); GATE 3 the reset state (option C's split: the TRANSPORT default cachedLength/endStep/globalWalk_/dnaLength == the cap; the LANE-WINDOW seeds == kDefaultPatternLen 16 — the stay-16 group, the cap move DEFERRED; the identity rows == the cap, bug 23); GATE 4 the default march (a len-7 lane free-runs (T-1)%7 under the cap transport — the cap arm; a default lane reads (T-1)%16; the playhead wraps at the cap); GATE 5 the authored 16/0 bit-identity (the two arms congruent); GATE 6 the mapped 10@2 fold (unchanged); GATE 7 PROMPT 2 (an L=7 R=3 voice row under a 16-step pattern reads the phrase-anchored fold (((T-1)%16 + 3) mod 7) — the macro row's twin identical — and R at EVERY global step 1: the cycle restarts from R each phrase; was the raw congruence's free-run); GATE 8 the dividing-L congruence (L=8 R=3 under 16: the phrase anchor congruent with the old law — the goldens hold); GATE 9 kVoiceSlots == 16 at every spelling (the class alias, the namespace hoist, VoiceBank's slots)
  "test_macro_lane_seed|$SE $GS $PE $CE"   # CC_FIX_MACRO_LANE_SEED_AND_CONSOLIDATION: the lane SEED (t0) law — Macro's inline seed had NO identity/mapped branch (the non-pass arm the raw GLOBAL walk wG+offG), so the Macro lanes folded over the global pattern; ONE laneSeed helper now holds the window->seed law for all three loops (East mono / East poly / Macro, the branch inside, identical — laneRecomp unchanged), the Macro mapped arm seeding the MACRO ROW's frame value. GATES: 1a the default global + identity row — the len-7 lane cycles its OWN 7 ((T-1)%7, the raw-tick polymeter free-run, UA_NONDIV's visible edge, UNCHANGED); 1b a mapped global 10@2 + identity row — the shared IDENTITY law (the bounded global walk folded by the lane's length, East's arm bit-identical); 1c an authored row 8@16 CYCLE — the lane hangs under the ROW's walk (((T-1)%8)%7, period 8, restarting at the row's wrap — pre-fix the free-run, the row ignored); 1d ABS1 — the row walks the FULL PHRASE (the ring, rho=(R-O)), the lane cycling its own 7; 2 EAST UNCHANGED — V1's mono lane with the same window == the Macro lane under BOTH globals (the helper reproduces the inline law; the deep byte-identity is the battery's UA_NONDIV golden + the position-holder suite); 3 SAME WINDOW -> SAME TICK — the drift-proof: V1's mono lane, EVERY follower V2..V16's lane and the Macro lane produce the SAME tick for every tick (the three seed sites, one law); 4 V1..V16 voice-independence (one global row); 5 the STRIP READ — macroFrameRead_ walks the authored window [16,24) (the row's walk — pre-fix the GLOBAL walk folded %16, bug 17's twin at this tier), R phase-shifting the walk (rho=R) with the lane inheriting the shifted frame value
  "test_macro_shared_state|$SE $GS $PE $CE"   # CC_PROMPT_MACRO_RESOLVER_AND_IDENTITY (LOR_EDITING bugs 19-21 + 23) — the MACRO-follower coverage the battery lacked, driven through VoiceResolver for ALL 16 voices incl. the mono slot: 1+3 the SHARED WINDOW (a Macro window O=16 L=8 — EVERY follower V1..V16 reads [16,24) with the identical period in lockstep; the mono slot's reads equal every other slot's — NOT special), 2 the SHARED MASK (a painted Macro mask gates every follower identically at every step; all-on passes everywhere, all-off mutes everywhere), 4 IDENTITY = the FULL voice window (the cap sentinel — was the literal 16; the identity voice unrestricted within the global window, the reads spanning all its cells; the global length changed mid-play leaves it unrestricted), 5 the GREP PIN (voiceLorIdentity + the two pass-through checks spell the named constant). RECORDS THE AUDIT: bugs 19-21's located cause (the raw-v liveness test) does NOT reproduce at the engine level — the loop's body resolves every live slot through effectiveVoiceLor, all followers share the window (gates 1-3 pass pre-fix); the PART-A resolver routing lands as the mandated hygiene with these gates proving the no-change; gate 4 is bug 23's failing-first reproduction
  "test_phase_tier|$SE $GS $PE"   # PHASE_TIER Stages 1+2 (docs/design/CC_PROMPT_PHASE_TIER.md + PHASE_TIER_PLAN.md): Stage 1 the transport phase span — span probes at 16 == the old p16*16 formula (bit-identical) and at 32 == p16*32 (the full-span D9 answer), the 32-step sweep covers ALL 32 steps with period 32 (FNV-1a digest equality, binary-exact pulse drive, the span wrap reads continuous-forward), the 16-span cycle lands k%16 every span (drift-rule consistency, no strobe), the bar-2 jump lands the MUSICAL step (the steps-32 kMaskRing-flip regression repair), the PHASE-STEP RULE (phaseStepFor == (startStep + absStep%len)%ring, always in-window, == absStep%16 at the default window — one rule both sites, snap + drift check). Stage 2 the subphase tier — UNPATCHED == process() exactly (the delegate, dirs neutral, reseed on a fresh cable), the QUOTIENT/REMAINDER pins at regionSize 1 bar AND 1 quarter (hand-computed seed probes incl. the wrapped transport input and the exact-boundary coarse; 8 regions across the two bars — the doc's own numbers), the transport's within-region fine motion QUANTISED AWAY (no position move, no edges), the subphase positions within its region continuously, the SIGN-PRODUCT MATRIX (fwd·fwd = forward, fwd·rev = backward, BOTH-reverse = FORWARD — the design doc's open check adjudicated intended; a flat tier carries no direction, the other tier's sign governs), the REGION LANDING through the EXISTING jump contract (jumpSteps == regionSixteenths (the replay unit is the STEP — the same number at 24), same sub-position in the new region, snap + drift rule accept it), NO-CROSS (a full subphase sweep through its ramp wrap never leaves the region's pulses — the floor mapping keeps it STRICT) and the wrap as a backward jump inside the region ("its jumps stay inside one bar"). Stage 3 the DISPLAY CONFIRM — the ring lamp + the inner-disc BAR LEDs read engine.stepIndex alone, so the pin asserts the display law under subphase: a full subphase sweep inside a bar region walks ONLY that bar's cells (the lap never flips — "no jumps between bars" made visible in the disc), the lap flips EXACTLY at the region landing (the clock-reading invariant), and the twice-around lamp feed stays coherent
  "test_position_holders|$SE $GS $PE $CE"   # POSITION_HOLDER_MODEL.md: the counter is DEAD — every walk owns a bounded phase — the pins: march equivalence vs the f(counter) closed forms (all four shapes, non-div 7/5/6, mono + per-voice), backward retraces forward, the re-anchor law (a mid-run len/dir change == f(t, newParams) exactly), the warm-phase law (pass-through → mapped → pass-through round-trip), the anchor law (reset / phase snap / the Beat-1 hard reset), the offset diff (a hand seed / the activation zeroing absorbed exactly), the write-site law (the transport's own position: ±1 on the events, verbatim on the refresh)
  "test_step_advance|$SE $GS $PE $CE"   # CC_PROMPT_GATE_STEP_ADVANCE_SPEC Stages 1+3 + CC_PROMPT_STEP_ADVANCE_CONSUMPTION Stage 2: the gate-mode probabilistic step-advance — decideAdvance table (incl. the Stage-2 bipolar branch, SOURCED: p<0 reachable, backward walk + wrap pinned, the candidacy/advance pair decoupled), the p=1 GOLDEN PIN (byte-identity with the pre-feature engine, digests captured pre-change), the trajectory == the mirrored rule per rise, the p=0 held repeat (no re-draw branch — Rodney 91b100c), the three-level main/ratchet/ghost hierarchy on the resolved ADVANCE columns behind subResLive, clock/phase untouched
  "test_subgate|$SE $GS $PE"
  "test_subgate_knobs|"   # header-only: the subgate-tier consumer-group bank + the three-way resolver (CC_PROMPT_STRAITS_SUBGATE_KNOBS_SPEC + STEP_ADVANCE_CONSUMPTION Stage 2) — byte-identity default, tier 1 / 2a-global / 2b-follow-main precedence, post-resolution delta clamp (per-decision range: [-1,+1] for ADVANCE, [0,1] for the rest), bank defaults, persistence round-trip (deltas session-lived), absent-keys ⟹ defaults, short/wrong-typed arrays
  "test_var_leg_rest|"
  "test_voice_channel_mapping|"   # header-only: pins poly output-channel(i+1)->V(i+2) and Lantern global-voice g->V(g+1) (the two off-by-ones: Rack showed row 2 missing / lane N drove row N+1 / V16 never output). RED on slot(i+1)/slot(pv_i+1).
  "test_voice_conventions|$SE $GS $PE"
  "test_voice_resolver|$SE $GS $PE"
  "test_warp_f_bank|"   # header-only: the per-voice warp-F variant bank (CC_PROMPT_02 — identity default, single-impl holders, branchless mix)
  "test_warp_stages|"   # header-only: FEEL-TEST MO rail-tilt + injected-atom stages (CC_PROMPT_TILT_AND_RAIL_FEELTEST) — neutral bit-exact, monotone, atom mass, tilt direction, edge continuity, persistence
  "test_phase_distribution|"   # header-only: the PHASE_DISTRIBUTION_SPEC acceptance battery — the gap stage's 8 spec tests (continuity/non-decreasing/G(1)=1 grid, width-zero BIT-identity, the a=0/b=1 clamp, the round trip, the jump at F_a/(1-p), the uniform rescale, atom-in-gap + the overlap rule) + the DUAL reading (the swap identity for EVERY member, atom<->gap exchange numeric, the bank's chained-CDF quantile, the slot-codec round-trip) + §10/§10b the EFFECTIVE plot law (the reading's guarded fill forms, the sign-sweep continuity incl. the −0.01 near-diagonal pin, the trace == warpedValue both signs, the fill integrates to ~1 — CC_FIX_PHASE_PLOT_NEGATIVE_WARP)
  "test_phase_distortion|"   # header-only: the PHASE-DISTORTION EXPANDER MODEL battery (PHASE_DISTRIBUTION_SPEC §14 + CC_PROMPT_PHASE_DISTORTION_EXPANDER) — G1 IDENTITY (a default store is eight identity sets; the primitive maps u to u BIT-EXACTLY over the sweep), G2 SAME INDEX (equal sets at different indices ⟹ bit-identical outputs), G3 DIFFERENT INDEX (the sets the readers read decide the output; the Sklar edges), G4 BLOCK CONSISTENCY (the Task-1 gate: two readers + a write BETWEEN blocks both flip at the SAME boundary, never one before the other; a mid-block write is invisible until the boundary; a multi-field commit never tears; the store's JSON round trip + the payload-less load), G6 SINGLE IMPLEMENTATION (phaseDistort(u, set) == WarpBank::warpedValue(u) BIT-EXACT over the roster x signed-w grid — the ONE extracted body), G7 SEMANTICS (the sign IS the reading vs the chain helpers directly, the ε clamp through the primitive, the atom plateau at ℓ, the [0,1] self-map range) + the CALIBRATION detent pins (the straight 24-PPQN list {96,48,24,12,6} pulses tiles the bar; the knob→detent map covers [0,1], 1.0 = bar)
  "test_phase_distortion_module|"   # header-only: the STANDALONE PHASE-DISTORTION MODULE battery (PHASE_DISTRIBUTION_SPEC §13 + CC_PROMPT_PHASE_DISTORTION_MODULE — the B comparison build, alongside A) — G1 IDENTITY the ADJUSTED gate (all-identity channels: out == the source SHIFTED ONE SAMPLE bit-exact over the ramp+wrap+jump+stall sweep; out(0)==0; unpatched stays 0; delegation alone stays raw), G2 CHANNEL SEPARATION (changing channel 3's set changes ONLY channel 3 — the others stay the bit-exact delayed raw, channel 3 == the primitive on its own set), G3 SAME OUTPUT SAME RESULT (two consumers of output k: two cores, same set + stream, bit-identical sample for sample), G4 SHARED PRIMITIVE (the core's output == phaseDistort(normPhase(delayed), cell) * phaseInMax BIT-EXACT over the roster x signed-w x u-grid — the ONE primitive, called not copied; the identity raw pass-through pinned), G6 A/B READY (B's stream == the expander build A's Mode E stream SHIFTED ONE SAMPLE, bit-exact over the roster x signed-w incl. 0 — the SAME PhaseEngine::normPhase/phaseInMax both sides share) + the §13 BLOCK CADENCE (the module's per-block snapshot at the kCtrlEvery=32 cadence: a write just before the boundary does NOT land mid-block, frame 32's boundary picks it up, the output follows the same frame)
  "test_warp_dist_mod|"   # header-only: the distribution send-bus fold (CC_PROMPT_SANDS_DISTRIBUTION_EXPANDER_SPEC) — flag-off/zero-plane neutrality bit-exact, global-vs-own reach, the delegated branch, the window clamp, the eval-side tilt clamp
  "test_sands_distribution_role|"   # header-only, rack-free: the distribution sub-expander's dual-role walk (host-on-first-hop, safe defaults, hop-through, the foreign boundary, first-host-wins, the depth guard, purity)
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

# ── plugin-TU compile smoke (the CI-build guard the rack-free battery cannot give) ──
# The unit tests compile the dsp headers rack-free; they NEVER see the widget-bearing
# plugin headers (MonsoonCantonment.hpp and friends need the real rack.hpp), so a scope
# bug there ships battery-green locally and only surfaces as a red CI build (2026-10-27:
# the containment widget's unqualified N_VERBS, red on all 3 platforms while the battery
# stayed 87/87). When a REAL Rack SDK is mounted at RACK_DIR (include/rack.hpp exists —
# always in CI; locally pass RACK_DIR=/path/to/Rack-SDK), -fsyntax-only every plugin TU
# with the Makefile's include set. Skips with a note when RACK_DIR is absent or carries
# only dep/include (the simde-only mount). Skipped by the name filter unless it matches
# "smoke".
smoke_fail=0
if [ -z "$filter" ] || [[ "smoke" == *"$filter"* ]]; then
  if [ -n "${RACK_DIR:-}" ] && [ -f "${RACK_DIR}/include/rack.hpp" ]; then
    echo "plugin-TU syntax smoke (RACK_DIR=${RACK_DIR}):"
    SMOKE_INCS="-Idep/include -isystem ${RACK_DIR}/include -isystem ${RACK_DIR}/dep/include"
    SMOKE_STD="-std=c++20 -Wno-c++17-extensions"
    case "$(uname -s)" in
      MINGW*|MSYS*|CYGWIN*) SMOKE_STD="$SMOKE_STD -D_USE_MATH_DEFINES -municode" ;;
    esac
    case "$(uname -m)" in
      x86_64|i686)    SMOKE_ARCH="-march=nehalem" ;;        # compile.mk ARCH_X64 baseline (SSE4.1/4.2)
      arm64|aarch64)  SMOKE_ARCH="-march=armv8-a+fp+simd" ;;
      *)              SMOKE_ARCH="" ;;
    esac
    smoke_idx=0; smoke_running=0
    for f in src/*.cpp src/dsp/engines/*.cpp src/dsp/managers/*.cpp src/dsp/gates/*.cpp; do
      extra=""
      case "$f" in *PatternEngine.cpp) case "$(uname -m)" in x86_64|i686) extra="-mavx" ;; esac ;; esac
      ( if ! g++ $SMOKE_STD $SMOKE_ARCH $extra $SMOKE_INCS -fsyntax-only "$f" 2>"$workdir/smoke_$smoke_idx.err"; then
          printf '%s\n' "$f" > "$workdir/smoke_$smoke_idx.bad"
        fi ) &
      smoke_idx=$((smoke_idx+1)); smoke_running=$((smoke_running+1))
      if [ "$smoke_running" -ge "$jobs" ]; then wait; smoke_running=0; fi
    done
    wait
    smoke_total=0
    for ((i=0; i<smoke_idx; i++)); do
      smoke_total=$((smoke_total+1))
      if [ -f "$workdir/smoke_$i.bad" ]; then
        printf "  \033[31m✗\033[0m  %s\n" "$(cat "$workdir/smoke_$i.bad")"
        grep -m 2 -E "error:" "$workdir/smoke_$i.err" | sed 's/^/       /'
        smoke_fail=$((smoke_fail+1))
      fi
    done
    if [ $smoke_fail -eq 0 ]; then
      printf "  \033[32m✓\033[0m  %d plugin TUs syntax-clean against the real SDK headers\n" "$smoke_total"
    fi
    echo "-----"
  elif [ -z "$filter" ]; then
    printf "  \033[33m?\033[0m  plugin-TU syntax smoke SKIPPED (no real SDK at RACK_DIR — point RACK_DIR at a Rack-SDK carrying include/rack.hpp to enable)\n"
    echo "-----"
  fi
fi

# ── array-guard lint (ARRAY_CONTAINER_AUDIT P-E) ──────────────────────────────
# Fail the battery on any NEW bare C-array declaration in src/dsp/** and src/ui/**
# outside test/lint_array_guard_allowlist.txt (char string buffers + label tables are
# exempt classes; the 137-entry baseline is the pre-gate residue outside the audit's
# named P-A/P-B/P-C scope). ARRAY_GUARD_STRICT=0 = local debug hatch (repo precedent).
# Skipped by the name filter unless it matches "array".
guard_fail=0
if [ -z "$filter" ] || [[ "array" == *"$filter"* ]]; then
  echo "array-guard (P-E):"
  if [ -z "$PYBIN" ]; then
    printf "  \033[33m?\033[0m  array-guard SKIPPED (no python found)\n"
  elif ! "$PYBIN" "$here/lint_array_guard.py"; then
    guard_fail=1
  fi
  echo "-----"
fi

if [ $fail -gt 0 ] || [ $audit_fail -ne 0 ] || [ $guard_fail -ne 0 ] || [ $smoke_fail -ne 0 ]; then
  [ $fail -gt 0 ] && echo "FAILED: ${failed_names[*]}"
  [ $audit_fail -ne 0 ] && echo "FAILED: anchor/bind audit"
  [ $guard_fail -ne 0 ] && echo "FAILED: array-guard (P-E)"
  [ $smoke_fail -ne 0 ] && echo "FAILED: plugin-TU syntax smoke"
  exit 1
fi
echo "all green"
