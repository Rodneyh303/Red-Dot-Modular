# ZAI PROMPT — Resolver Step 4: DIRECTION as `<DirPolicy>` — the LAST seam closes, the manifest goes EMPTY

Step 4 of 4 in the SandsModifierResolver build order (SANDS_MODIFIER_RESOLVER_DESIGN.md).
Spread (step 1), warp (step 2) and LOR (step 3) ride the skeleton. Direction is the
last hand-rolled delegation shape — and step 4 also closes the LAST manifest entry
(`DIR-SLOT0-ROW-INERT`), so **`knownSeams()` becomes EMPTY**: the stage-0 goal state
("manifest only ever shrinks" reaches its terminus; the harness itself anticipates it
— test_lane_voice_uniformity.cpp:741 prints "manifest EMPTY, all cells uniform").

This step is **byte-identical everywhere EXCEPT ONE unconsumed store row** (see the
honesty block): the direction values the audio thread sees do not change. The two bugs
the design doc hung on step 4 ("VAR/LEG direction", "stale-reset") are BOTH already
resolved on this branch by the lane_direction_homes migration — reconciled under
VERIFIED FACTS; do not re-fix them here.

## THE TASK (one commit)

1. Add `DirPolicy` to `src/dsp/SandsModifierResolver.hpp` (POLICIES section) + a
   header-side `resolveDirItem(...)` wiring helper (beside resolveWarpCell /
   resolveLorItem).
2. Route BOTH direction pushes through it: the POLY push
   (MonsoonExpanderManager.cpp, the `laneDirVPending_` block — the dead VAR/LEG
   else-branch is deleted as dead code) and the MONO push (the `laneDirPending_`
   block via `monoDirAuthority` — only the macroGlobal-vs-field selection routes;
   the param-backed arm stays call-site).
3. Widen the promotion loop to slot 0 (SequencerEngine.cpp, BOTH `advancePlayhead`
   sites: the StepEdge block ~:189 and the Phrase block ~:306) — `for (int v = 1; ...)`
   → `for (int v = 0; ...)`. This closes `DIR-SLOT0-ROW-INERT`.
4. Harness: U7's slot-0 sub-check flips polarity (asserts promotion now); the
   manifest entry + `seam_dir_slot0` are DELETED; U3/U7 stale section comments and
   the file-header "RED on today's code" line are refreshed (comment-only — this
   step finishes the harness's story, so its header may finally say all-green).
5. New `test/test_resolver_dir_bridge.cpp` (exhaustive — the selection space is
   tiny); suites 68→69 ALL green; UNIFORMITY_STRICT=1 exit 0 with an EMPTY manifest;
   plugin build SUCCESS (both manager cpps stay rig-invisible).

## THE SPEC

### DirPolicy — PURE SELECTION, no blend (design doc line 42)

- `Value = float` — the RAW store direction value (0..3 float: 0=Forward, 1=Reverse,
  2=Pendulum, 3=PingPong — `SequencerEngine::LaneDir`, enum class : uint8_t, at
  SequencerEngine.hpp:214). Value stays FLOAT because the old algebra selects on the
  raw floats and rounds ONCE after selection:
  `(int)std::lround(math::clamp(dirVal, 0.f, 3.f))`. Do NOT pre-round the sources:
  lround is monotone so round-then-select ≡ select-then-round, but keeping the old
  order verbatim is the byte-identical law — select in the resolver, round in the
  helper, exactly like the old line. Value is NOT `LaneDir` itself: the resolver
  header must stay engine-include-free (layering: engines include dsp-root, never
  the reverse; a second dir enum is worse than a documented float+mapping).
- `Extra = {}` (struct Extra {};).
- `scaleCv = identity`, `addCv/addSend = v + c / v + s` — concept-required members,
  structurally UNUSED by direction (no CV/send/tap exists for it). The wiring leaves
  ownCv/eastCv disconnected and macroSend=0.
- `applyDelegated(mb, td, e) { return mb; }` — **EXPLICIT pure selection, td
  structurally ignored** (NOT `mb + td` like spread/warp: direction has no delta
  term; a comment must say a future direction-modulation feature would be a NEW
  policy member, never a silent td). The wiring still leaves pre/post/tap at
  defaults, but the policy must not DEPEND on that.
- `constrain(v, e) = max(0.f, min(3.f, v))` — the max/min form (NaN convention, one
  per file). Identity on valid inputs: the old code clamped the same [0,3] after
  selection; the resolver now owns that clamp.
- `static_assert(SandsModifierPolicy<DirPolicy>);`

### `resolveDirItem` — the ONE wiring, header-side

```cpp
// Returns the FINAL int (the old (int)lround(clamp(sel, 0, 3)) — the round+cast is
// part of the moved algebra; both pushes become one-token thin).
inline int resolveDirItem(bool delegated, float ownDir, float macroGlobalDir);
```

Body: fills `SandsModifierResolver<DirPolicy>::Inputs` — `in.delegated = delegated;
in.base = ownDir; in.macroBase = macroGlobalDir;` — and returns
`(int)std::lround(resolved)`. All other Inputs fields at defaults (macroPresent
irrelevant: the delegated branch never reads send terms; the free path adds none).

### The POLY push rewrite (MonsoonExpanderManager.cpp, `laneDirVPending_` block)

Old (current numbering ~:384-399; line numbers WILL have shifted if step 3 landed —
anchor by symbol `laneDirVPending_[v][l] =`):

```cpp
float dirVal;
if (l < dotModular::SandsGrid::POLY_LANES) {
    dirVal = (mmOwn->getMacroOwn(v, l) > 0.5f)
           ? mmOwn->getLaneDir(v, l)       // East owns → voice's own (cached)
           : mmOwn->getGlobalDir(l);       // Macro owns → Macro's global
} else {
    dirVal = mmOwn->getLaneDir(v, l);      // VAR/LEG: always voice's own  ← DEAD
}
int dv = (int)std::lround(math::clamp(dirVal, 0.f, 3.f));
engine.laneDirVPending_[v][l] = (SequencerEngine::LaneDir)dv;
```

New:

```cpp
const bool delegated = (mmOwn->getMacroOwn(v, l) > 0.5f);
engine.laneDirVPending_[v][l] = (SequencerEngine::LaneDir)
    redDot::resolveDirItem(delegated, mmOwn->getLaneDir(v, l), mmOwn->getGlobalDir(l));
```

- The `if (l < POLY_LANES) / else` split is **DEAD CODE**: the loop runs
  `l < NUM_STRANDS` and `NUM_STRANDS == POLY_LANES == 7` (SANDS CONSOLIDATION
  unified them; ui/SandsGrid.hpp static_asserts pin the equality). The else-arm
  never executes. Deleting it is NOT a behaviour change — but the stale comment
  above it ("Lanes 4/5 (VAR/LEG) have no Macro ownership") must go with it,
  replaced by a comment stating the uniform all-7-lane routing and that VAR/LEG
  follow Macro IFF their store ownership flag says so (they are full lanes).
- Keep the enclosing guards (`if (mmOwn)`, the loop bounds) untouched.

### The MONO push rewrite (`laneDirPending_` block, via monoDirAuthority)

Old shape (~:163-181): `monoDirAuthority(l)` returns a `MonoDirSrc`; the block then
branches: `src.isField()` + `src.macroGlobal` → `getGlobalDir(eastMonoLane)`;
`src.isField()` !macroGlobal → `getLaneDir(kMonoLaneDirRow, eastMonoLane)`;
`src.paramId >= 0` → direct param read. Route ONLY the field-backed selection
through the resolver:

```cpp
engine.laneDirPending_[l] = (SequencerEngine::LaneDir)
    redDot::resolveDirItem(src.macroGlobal,
                           mm->getLaneDir(Monsoon::kMonoLaneDirRow, src.eastMonoLane),
                           mm->getGlobalDir(src.eastMonoLane));
```

The `paramId >= 0` arm is a SOURCE-TYPE dispatch (which module owns the value), not
a Macro/East delegation selection — it stays call-site, verbatim. The
`if (!src.valid()) continue;` reset-fallback guard stays verbatim (that IS the
reset half working; see FACTS).

### `monoDirAuthority`'s `lane >= 4` — PRESERVE VERBATIM, FLAG, DO NOT FIX

`const bool varleg = (lane >= 4);` (MonsoonExpanderManager.cpp:~34) is an
OLD-ORDER residue: it was written when VAR/LEG were lanes 4,5 (pre-QMIX). After the
QMIX insertion the engine order is MEL/OCT/QMIX/REST/ACC/VAR/LEG (PL_VARIATION=5,
PL_LEGATO=6), so the check's set is now **{ACC(4), VAR(5), LEG(6)}** — ACC is
included, which the name/intent ("Macro has no such lane") no longer matches, since
Macro DOES have an ACCENT lane. Changing it to `lane >= PL_VARIATION` would make
V1's ACCENT direction source Macro-global instead of East's V1 field — a REAL
behaviour change on a lane CC_PROMPT_08's report describes as "DOES follow"
(contradiction that needs Rack eyes, not a blind edit). **Keep `lane >= 4` exactly;
add a comment naming the discrepancy; list it in the exit report as the one open
stale-index artifact — Rodney-decidable, CC_PROMPT_08 territory.** Rename NOTHING.

### The promotion widening — closing DIR-SLOT0-ROW-INERT

SequencerEngine.cpp `advancePlayhead`, BOTH blocks (StepEdge ~:189, Phrase ~:306):

```cpp
for (int v = 1; v <= numPolyVoices; ++v) laneDirV_[v][l] = laneDirVPending_[v][l];
```

→ `for (int v = 0; v <= numPolyVoices; ++v) ...` — both sites. Notes that must go
in the comment: slot 0's poly row now promotes for STORE consistency (the manager
resets+pushes pending for all 16 slots; the engine silently dropped slot 0 — the
seam). NO consumer reads `laneDirV_[0][*]`: the recompute loops (~:248, ~:342) also
run `v = 1..` and are LEFT UNTOUCHED (widening them would touch laneTickV_/laneSignV_
slot-0 semantics for zero readers — out of scope; note it), and V1's audio path reads
the MONO channel `laneDir_[l]` — so this change cannot move the audio. U7's watchdog
sub-check flips from asserting-inert to asserting-promoted (below).

### The harness edits (the ONLY test edits allowed)

1. U7 (`test_lane_voice_uniformity.cpp:~527`): the slot-0 else-branch flips from
   `expectCell(se.laneDirV_[0][l0] == LaneDir::Forward, ..., "slot-0 poly row
   promoted (unexpected — manifest says inert)")` to asserting **Reverse** (the
   promoted value) with the message updated ("slot-0 poly row did not promote").
2. The manifest: delete the `DIR-SLOT0-ROW-INERT` entry AND the `seam_dir_slot0`
   matcher function; leave `knownSeams()` returning an EMPTY vector with the
   closing comment: "manifest EMPTY — the stage-0 goal state (every seam closed;
   the only-shrink rule reaches its terminus). New seams must NEVER be added; a
   red cell is now always a bug."
3. Comment-only refresh: U7's section header ("Slot 0's poly row is INERT on
   HEAD → KNOWN SEAM" → promoted-uniform now), U3's stale warp-consumer wording
   (step-2 residual), and the file-header "RED on today's code" line (step-2
   residual) → all-green description. NO logic edits anywhere else in the harness.

### The bridge test (test_resolver_dir_bridge.cpp)

Header-only, Rack-free, registered as `"test_resolver_dir_bridge|"`. The selection
space is TINY — EXHAUSTIVE, not statistical:

- ORACLE (frozen, verbatim): `dirOracle(delegated, ownDir, globalDir)` =
  `(int)std::lround(math::clamp(delegated ? globalDir : ownDir, 0.f, 3.f))` —
  duplicating the old poly line (the mono line's field arms reduce to the same
  shape — note that in the header comment). New-code side: `resolveDirItem(...)`.
- Grid: delegated ∈ {false,true} × ownDir/globalDir ∈ {0,1,2,3, 0.5,1.5,2.5,
  3.5(out), −0.5(out), 0.4999999, 2.5000001} — all pairs (~242 cases), asserting
  INT equality AND same-BITS of the pre-round float (expose via a float-returning
  twin helper, same pattern as the LOR brief's pin-both-layers).
- Rounding-boundary anchors: lround half-cases (2.5 → 3), out-of-window clamps
  (−0.5 → 0, 3.5 → 3), and the delegated-arm ignoring ownDir entirely (pure
  selection: any ownDir with delegated=true ⇒ same result).
- A small seeded fuzz (≥10k, the pattern's sake) over the full Inputs shape.
- Coverage counters asserted; print seed + totals.

## WHAT IS BIT-IDENTICAL vs THE ONE DELIBERATE CHANGE (honesty block)

BIT-IDENTICAL: every direction VALUE the pending stores receive — poly all 7 lanes
(the dead else never executed), mono all arms (field-macroGlobal, field-east,
param), the reset half, the round+cast order. No audio-path change: direction
consumers (recompute/sign loops) read `laneDirV_[v]` from v=1 and V1 reads the mono
`laneDir_[l]` — unchanged.
THE ONE DELIBERATE CHANGE: `laneDirV_[0][l]` (slot 0's poly row) now PROMOTES on
advancePlayhead — an UNCONSUMED store row (no reader exists; U7 pins it). This
closes the seam and empties the manifest.

## VERIFIED FACTS (grounded 2026-10-20 at bb7d37b9 — re-verify if HEAD moved; step 3
will have shifted MonsoonExpanderManager.cpp line numbers — anchor by symbol)

- Engine lane order: PL_MELODY=0, PL_OCTAVE=1, PL_QMIX=2, PL_REST=3, PL_ACCENT=4,
  PL_VARIATION=5, PL_LEGATO=6 (SequencerEngine.hpp:356). NUM_STRANDS ==
  POLY_LANES == MONO_LANES == EAST_LANES == 7 (ui/SandsGrid.hpp, static_asserted).
- Poly push: MonsoonExpanderManager.cpp, `laneDirVPending_[v][l] =` inside
  `if (mmOwn)` / `for l < NUM_STRANDS`; the `l < POLY_LANES` ternary-guard is
  always-true (dead else). Ownership authority here is the STORE flag
  `getMacroOwn(v,l) > 0.5f` (Monsoon.hpp:970, `macroOwn[112]`, written by the East
  UI's per-lane owner switches — StraitsEastSandsVisual.cpp:429/:436/:770, all 7
  lanes) — NOT topo.owner; the resolver CONSUMES the boolean either way
  (membership rule). Do not "unify" the authority in this step.
- Mono push: the `laneDirPending_[l]` block; `monoDirAuthority` (:~29-41) with
  `varleg = (lane >= 4)` (stale set {ACC,VAR,LEG} — preserve, flag); the param arm;
  `if (!src.valid()) continue;` = the reset fallback (Forward from reset).
- Reset half EXISTS (already fixed): reset-then-push at MonsoonExpanderManager.cpp
  ~:141-150 ("The reset is the half that was missing while the engine was
  direction's home"), plans/lane_direction_homes.md landed e199431c BEFORE the
  design doc 559a276f — so the design doc's "missing reset half, :142-143" and
  "stale-reset bug" describe the PRE-homes state. Reconciled: nothing to fix.
- VAR/LEG poly direction already routes through the ownership ternary (dead else) —
  the design doc's "VAR/LEG direction bug" as a POLY-path defect is already
  resolved by the consolidation; the remaining live question is the MONO `lane >= 4`
  set (above, flagged, not fixed).
- Promotion loops: SequencerEngine.cpp StepEdge ~:189 + Phrase ~:306 (`v = 1..`);
  recompute loops ~:248/:342 (`v = 1..`, untouched); reset() :112 writes
  `laneDirV_[v][s] = Forward` — check its v-bound when editing (if it starts at 0,
  slot-0 pending/committed rows are both reset-clean; if at 1, note it — either way
  no consumer).
- U7 harness: test_lane_voice_uniformity.cpp:~494-537 (section header ~:494, the
  slot-0 sub-check ~:527); manifest entry at :83-89 + `seam_dir_slot0` matcher at
  ~:76; the empty-manifest state is anticipated at :741.
- The harness partition logic works with an empty manifest (failures partition
  against zero entries ⇒ every failure is "UNEXPECTED" ⇒ red; today's tree must be
  all-green before AND after this step).

## HOW TO RUN

```
cd Red-Dot-Modular && git pull --ff-only
RACK_DIR=<sdk> bash test/run_all.sh                  # 69 suites, ALL green
UNIFORMITY_STRICT=1 RACK_DIR=<sdk> bash test/run_all.sh   # exit 0, "manifest EMPTY, all cells uniform"
make RACK_DIR=<sdk> -j16                             # plugin build SUCCESS — MANDATORY
```

## COMMIT + EXIT REPORT

One commit: `refactor: DirPolicy — both direction pushes ride SandsModifierResolver; DIR-SLOT0-ROW-INERT closed, manifest EMPTY (step 4)`.

Report: suite count + anchors (east 99 / macro 126 / keppel 19 / causeway 55);
STRICT output line (must show the empty-manifest wording); bridge totals; plugin
build result; the `lane >= 4` flag (verbatim-preserved, {ACC,VAR,LEG} vs intent,
Rodney-decidable); the reset()-v-bound observation; any fact that moved vs this
brief.

## NOT THIS TASK

- The mono `lane >= 4` semantic change (ACC following Macro-global on V1) — flagged
  only; CC_PROMPT_08 territory, Rodney-decidable.
- CC_PROMPT_08's contract test / Rack feel-test — its own diagnose-first prompt
  (feat/warp-ui era); this step's uniform poly routing is the construction-level
  answer, the store-level verification stays separate.
- Widening the recompute loops to v=0 (no readers; laneTickV_/laneSignV_ slot-0
  semantics untouched).
- θ/clamp policies and the voiceLor policy (the per-voice LOR level) — PLANNED framework
  members, PLANS ONLY (sketch :172 `VoiceLorPolicy`; design doc build-order step 5). The
  V1 mono LOR copies (MonsoonSandsManager.cpp) stay untouched this step; their unification,
  if ever, belongs to that future policy's own brief — not here.
- Concept-signature changes, feat/warp-ui merge, any new manifest entry.
